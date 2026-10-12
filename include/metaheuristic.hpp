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

        int LOOP = 10;

        /**
         * @brief Represents a state transition within the neighborhood search.
         */
        struct Move {
            int type = 0;            ///< @brief 0: Invalid, 1: Shift, 2: Swap.
            int device1_id = -1;     ///< @brief ID of the primary device.
            int device2_id = -1;     ///< @brief ID of the secondary device (for swaps).
            int old_server1_id = -1; ///< @brief Original server of device 1.
            int old_server2_id = -1; ///< @brief Original server of device 2 (for swaps).
            int new_server1_id = -1; ///< @brief New server for device 1.
            int new_server2_id = -1; ///< @brief New server for device 2.
            double delta_cost = 0.0; ///< @brief The change in total penalty cost.
        };

        inline Move apply_neighbor(core::Devices& devices, core::Servers& servers, const id_pair_vector& sorted_servers, const i_vector& devices_served, int tries = 5) {
            Move move;
            
            if (randomness::random_number<double>(0.0, 1.0) < 0.5) {
                // ==========================================
                // ACTION 1: SHIFT
                // ==========================================
                move.type = 1;
                move.device1_id = randomness::random_number<int>(1, static_cast<int>(devices.size() - 1));
                
                core::Device& device = devices[move.device1_id];
                move.old_server1_id = device.server_id;

                bool overloaded = false;
                if (sorted_servers[0].second <= 0.5) {
                    overloaded = true;
                }

                int count = 0;
                while(count <= tries) {
                    int i = count;
                    count++;

                    if (overloaded) i = randomness::random_number<int>(0, static_cast<int>(sorted_servers.size() - 1));
                    if (i == device.server_id) continue;
                    
                    int target_server_id = sorted_servers[i].first;
                    if (target_server_id == device.server_id) continue;
                    core::Server& new_server = servers[target_server_id];
                    
                    if (new_server.can_serve(device)) {
                        if (device.served) {
                            servers[move.old_server1_id].rmv_service(device);
                            move.delta_cost = 0.0;
                        } else {
                            move.delta_cost = -device.cost;
                        }
                        
                        new_server.add_service(device);
                        move.new_server1_id = target_server_id;
                        return move;
                    }
                }

                move.type = 0; 
                return move;
                
            } else {
                // ==========================================
                // ACTION 2: SWAP
                // ==========================================
                move.type = 2;

                if (devices_served.size() < 2) {
                    move.type = 0;
                    return move;
                }

                int rand_idx1 = randomness::random_number<int>(0, static_cast<int>(devices_served.size() - 1));
                int rand_idx2 = randomness::random_number<int>(0, static_cast<int>(devices_served.size() - 1));
                
                move.device1_id = devices_served[rand_idx1];
                move.device2_id = devices_served[rand_idx2];
                
                core::Device& d1 = devices[move.device1_id];
                core::Device& d2 = devices[move.device2_id];

                if (d1.id == d2.id || d1.server_id == d2.server_id) {
                    move.type = 0; 
                    return move;
                }

                move.old_server1_id = d1.server_id;
                move.old_server2_id = d2.server_id;

                core::Server& s1 = servers[move.old_server1_id];
                core::Server& s2 = servers[move.old_server2_id];

                s1.rmv_service(d1);
                s2.rmv_service(d2);

                if (s1.can_serve(d2) && s2.can_serve(d1)) {
                    s1.add_service(d2);
                    s2.add_service(d1);
                    
                    move.new_server1_id = s2.id;
                    move.new_server2_id = s1.id;
                    move.delta_cost = 0.0; 
                    
                    return move;
                } else {
                    s1.add_service(d1);
                    s2.add_service(d2);
                    
                    move.type = 0;
                    move.delta_cost = 0.0;
                    return move;
                }
            }
        }
    
        /**
         * @brief Reverts a previously applied neighborhood move.
         * @param devices The global state vector of devices.
         * @param servers The global state vector of servers.
         * @param move The Move structure containing the transition details.
         */
        inline void revert_neighbor(core::Devices& devices, core::Servers& servers, const Move& move) {
            if (move.type == 0) return;

            if (move.type == 1) { 
                core::Device& device = devices[move.device1_id];
                
                if (move.new_server1_id != -1) {
                    servers[move.new_server1_id].rmv_service(device);
                }
                if (move.old_server1_id != -1) {
                    servers[move.old_server1_id].add_service(device);
                }
            } 
            else if (move.type == 2) { 
                core::Device& d1 = devices[move.device1_id];
                core::Device& d2 = devices[move.device2_id];
                
                servers[move.new_server1_id].rmv_service(d1);
                servers[move.new_server2_id].rmv_service(d2);
                
                servers[move.old_server1_id].add_service(d1);
                servers[move.old_server2_id].add_service(d2);
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
            if (!metrics_ptr) {
                std::cerr << "Error: Invalid metrics type for heuristic solver.\n";
                return;
            }

            core::Devices best_devices = state.devices;
            core::Servers best_servers = state.servers;
            
            double best_cost = metrics_ptr->outputs.total_penalty_cost;
            double current_cost = best_cost;
            
            double T = metrics_ptr->temperature;
            double alpha = metrics_ptr->alpha;
            
            auto start_chrono = std::chrono::high_resolution_clock::now();

            while (T > ZERO) {
                for (int i = 0; i < LOOP; i++) { 
                    id_pair_vector sorted_servers = core::sort_servers_by_free(state.servers);

                    i_vector devices_served;
                    devices_served.reserve(state.devices.size());
                    for(const auto& d : state.devices) {
                        if(d.served) devices_served.push_back(d.id);
                    }

                    Move move = apply_neighbor(state.devices, state.servers, sorted_servers, devices_served);
                    if (move.type == 0) continue;

                    double neighbor_cost = current_cost + move.delta_cost;
                    double delta = neighbor_cost - current_cost;

                    if (delta < 0 || (randomness::random_number(0.0, 1.0) < std::exp(-delta / T))) {
                        current_cost = neighbor_cost;

                        if (current_cost < best_cost) {
                            i = 0;
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
            metrics_ptr->outputs.execution_time += std::chrono::duration<double>(end_chrono - start_chrono).count();
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
            metrics_ptr->run_id, 
            metrics_ptr->inputs.devices_count, 
            metrics_ptr->inputs.servers_ec_count, 
            metrics_ptr->inputs.servers_cc_count
        );

        heuristic::boot(initial_state);
        metrics::calculate_metrics(initial_state);
        metrics::save_results(initial_state, false);

        state.devices = initial_state.devices;
        state.servers = initial_state.servers;
        metrics_ptr->inputs = initial_state.stats->inputs;
        metrics_ptr->outputs = initial_state.stats->outputs;

        if (metrics_ptr->algorithm == metrics::Algorithm::simulated_annealing) {
            simulated_annealing(state);
        } else {
            std::cerr << "Error: Unknown metaheuristic algorithm type.\n";
        }
    }
}