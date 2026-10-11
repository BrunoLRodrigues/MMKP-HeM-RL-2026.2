#pragma once

#include "mmkp.hpp"
#include "heuristic.hpp"
#include "tools.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

/**
 * @brief Encapsulates meta-heuristic algorithms for network resource allocation.
 * @details Provides advanced optimization techniques, such as Simulated Annealing, 
 * to iteratively improve initial heuristic solutions by exploring neighborhood structures.
 */
namespace metaheuristic {

    namespace {

        /**
         * @brief Represents a state transition (move) within the neighborhood search.
         */
        struct Move {
            int device_idx = -1;      ///< @brief The index of the device being reassigned.
            int old_server_idx = -1;  ///< @brief The ID of the server originally serving the device.
            int new_server_idx = -2;  ///< @brief The ID of the proposed new server (-1 for dropped, -2 for invalid).
            double delta_penalty = 0.0; ///< @brief The change in the total penalty cost.
        };

        /**
         * @brief Attempts to find and apply a valid neighborhood move for a random device.
         * @details Selects a random device. If served, it has a 50% probability of being dropped 
         * to free up resources and escape local optima. Otherwise, it attempts to shift or assign 
         * the device to a new capable server.
         * 
         * @param devices The global state vector of devices.
         * @param servers The global state vector of servers.
         * @return A Move struct detailing the applied transition.
         */
        inline Move apply_neighbor(core::Devices& devices, core::Servers& servers) {
            Move move;
            move.device_idx = random::random_number<int>(1, static_cast<int>(devices.size() - 1));
            core::Device& device = devices[move.device_idx];

            move.old_server_idx = device.id_server;

            if (device.served && random::random_number<double>(0.0, 1.0) < 0.5) {
                servers[move.old_server_idx].rmv_service(device);
                move.new_server_idx = -1; 
                move.delta_penalty = device.cost; 
                return move;
            }

            auto servers_shuffled = random::shuffled_range<int>(1, static_cast<int>(servers.size() - 1));
            for (int s_idx : servers_shuffled) {
                if (s_idx == device.id_server) continue;
                
                core::Server& new_server = servers[s_idx];
                if (new_server.can_serve(device)) {
                    if (device.served) {
                        servers[move.old_server_idx].rmv_service(device);
                        move.delta_penalty = 0.0;
                    } else {
                        move.delta_penalty = -device.cost;
                    }
                    
                    new_server.add_service(device);
                    move.new_server_idx = s_idx;
                    return move;
                }
            }

            move.new_server_idx = -2; 
            return move;
        }
    
        /**
         * @brief Reverts a previously applied neighborhood move.
         * @details Restores the device and server states to their exact configurations 
         * prior to the execution of the specified move.
         * 
         * @param devices The global state vector of devices.
         * @param servers The global state vector of servers.
         * @param move The Move structure containing the transition details to be undone.
         */
        inline void revert_neighbor(core::Devices& devices, core::Servers& servers, const Move& move) {
            if (move.new_server_idx == -2) return;

            core::Device& device = devices[move.device_idx];

            if (move.new_server_idx != -1) {
                servers[move.new_server_idx].rmv_service(device);
            }

            if (move.old_server_idx != -1) {
                servers[move.old_server_idx].add_service(device);
            }
        }

        /**
         * @brief Executes the Simulated Annealing (SA) optimization algorithm.
         * @details Iteratively explores the solution space by applying random neighborhood moves. 
         * It accepts worse solutions with a probability dependent on the current temperature 
         * to escape local optima, cooling down progressively based on the alpha factor.
         * 
         * @param state The unified simulation state containing devices, servers, and metrics.
         */
        inline void simulated_annealing(metrics::Result& state) {
            auto metrics_ptr = std::dynamic_pointer_cast<metrics::MetaHeuristicMetrics>(state.stats);
            if (!metrics_ptr) return;

            auto start_chrono = std::chrono::high_resolution_clock::now();

            core::Devices best_devices = state.devices;
            core::Servers best_servers = state.servers;
            
            double current_cost = 0.0;
            for (size_t d = 1; d < state.devices.size(); ++d) {
                if (!state.devices[d].served) current_cost += state.devices[d].cost;
            }
            double best_cost = current_cost;

            double T = metrics_ptr->temperature;
            double alpha = metrics_ptr->alpha;
            int markov_chain_length = static_cast<int>(state.devices.size()); 

            while (T > 1e-3) {
                for (int i = 0; i < markov_chain_length; ++i) { 
                    Move move = apply_neighbor(state.devices, state.servers);
                    if (move.new_server_idx == -2) continue;

                    double neighbor_cost = current_cost + move.delta_penalty;
                    double delta = neighbor_cost - current_cost;

                    if (delta < 0 || (random::random_number(0.0, 1.0) < std::exp(-delta / T))) {
                        current_cost = neighbor_cost;
                        if (current_cost < best_cost) {
                            best_cost = current_cost;
                            best_devices = state.devices;
                            best_servers = state.servers;
                        }
                    } else {
                        revert_neighbor(state.devices, state.servers, move);
                    }
                }
                T *= alpha;
            }
            
            state.devices = std::move(best_devices);
            state.servers = std::move(best_servers);

            auto end_chrono = std::chrono::high_resolution_clock::now();
            metrics_ptr->outputs.execution_time_sec += std::chrono::duration<double>(end_chrono - start_chrono).count();
        }   
    }

    /**
     * @brief Initializes and launches the selected meta-heuristic algorithm.
     * @details Generates an initial solution using a specified base heuristic 
     * and then applies the configured meta-heuristic to optimize the allocation further.
     * 
     * @param state The unified simulation state to be optimized.
     */
    inline void boot(metrics::Result& state) { 
        if (!state.stats) return;

        auto metrics_ptr = std::dynamic_pointer_cast<metrics::MetaHeuristicMetrics>(state.stats);
        if (!metrics_ptr) {
            std::cerr << "Error: Invalid metrics type for meta-heuristic solver.\n";
            return;
        }

        metrics::Result initial_state = state;
        initial_state.stats = std::make_shared<metrics::HeuristicMetrics>(
            metrics_ptr->heuristic, 
            metrics_ptr->run_number, 
            metrics_ptr->inputs.devices_count, 
            metrics_ptr->inputs.servers_ec_count, 
            metrics_ptr->inputs.servers_cc_count
        );

        heuristic::boot(initial_state);

        state.devices = initial_state.devices;
        state.servers = initial_state.servers;

        if (metrics_ptr->algorithm_name == metrics::Algorithm::simulated_annealing) {
            simulated_annealing(state);
        } else {
            std::cerr << "Error: Unknown metaheuristic algorithm type.\n";
        }
    }
}