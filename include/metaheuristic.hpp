/**
 * @file metaheuristic.hpp
 * @brief Meta-heuristic optimization algorithms and neighborhood operators for MMKP.
 * @details Implements Simulated Annealing (SA) with incremental delta evaluation and
 * shift/insert/swap neighborhood moves to refine initial constructive heuristic solutions.
 */

#pragma once

#include "mmkp.hpp"
#include "heuristic.hpp"
#include "tools.hpp"

/**
 * @namespace metaheuristic
 * @brief Encapsulates meta-heuristic algorithms for network resource allocation.
 * @details Provides trajectory-based optimization techniques, specifically Simulated Annealing, 
 * to iteratively improve initial heuristic solutions by exploring shift, insertion, and 
 * 1-for-1 swap neighborhood structures.
 */
namespace metaheuristic {

    namespace {

        /**
         * @brief Number of inner-loop iterations (Markov chain length) executed per temperature level.
         */
        constexpr int LOOP = 10;

        /**
         * @struct Move
         * @brief Represents a reversible state transition within the neighborhood search.
         */
        struct Move {
            bool valid = false;      ///< @brief Indicates if a feasible move was found and applied.
            int device_id = -1;      ///< @brief ID of the primary device involved in the move.
            int old_server_id = -1;  ///< @brief Original server ID of the primary device (`-1` if previously unserved).
            int new_server_id = -1;  ///< @brief Target server ID where the primary device was allocated.
            int swapped_id = -1;     ///< @brief ID of the secondary device evicted in a swap (`-1` if not a swap).
            double delta_cost = 0.0; ///< @brief Net change in total penalty cost resulting from the move.
        };

        /**
         * @brief Applies a shift, insert, or swap neighborhood move to the current state.
         * @details Selects a random device and attempts to allocate or shift it across random 
         * servers (including a fallback check on the last server for unserved devices). If direct 
         * allocation fails for an unserved device, attempts a 1-for-1 swap with a currently served device.
         * 
         * @param devices The global state vector of devices.
         * @param servers The global state vector of servers.
         * @param tries The maximum number of random candidate attempts.
         * @return Move A structure containing the applied transition details and cost variation.
         */
        inline Move apply_neighbor(core::Devices& devices, core::Servers& servers, int tries = 10) {
            Move move;
            const int max_dev = static_cast<int>(devices.size() - 1);
            const int max_srv = static_cast<int>(servers.size() - 1);

            move.device_id = randomness::random_number<int>(1, max_dev);
            core::Device& device = devices[move.device_id];
            move.old_server_id = device.server_id;

            int count = 0;
            while (count < tries) {
                int i = (!device.served && count == tries - 1) 
                        ? max_srv 
                        : randomness::random_number<int>(1, max_srv);
                
                if (i == device.server_id) continue;
                count++;
                
                core::Server& new_server = servers[i];
                
                if (new_server.can_serve(device)) {
                    if (device.served) {
                        servers[move.old_server_id].rmv_service(device);
                        move.delta_cost = 0.0;
                    } else {
                        move.delta_cost = -device.cost;
                    }
                    
                    new_server.add_service(device);
                    move.new_server_id = i;
                    move.valid = true;
                    return move;
                }
            }

            if (!device.served) {
                for (int t = 0; t < tries; t++) {
                    int cand_id = randomness::random_number<int>(1, max_dev);
                    core::Device& cand = devices[cand_id];
                    if (!cand.served) continue;

                    int srv_id = cand.server_id;
                    core::Server& target_server = servers[srv_id];

                    target_server.rmv_service(cand);
                    if (target_server.can_serve(device)) {
                        target_server.add_service(device);
                        move.new_server_id = srv_id;
                        move.swapped_id = cand_id;
                        move.delta_cost = cand.cost - device.cost;
                        move.valid = true;
                        return move;
                    }
                    target_server.add_service(cand);
                }
            }

            return move;          
        }
    
        /**
         * @brief Reverts a previously applied neighborhood move in \f$O(\log N)\f$ time.
         * @details Restores the exact server allocations and resource supplies prior to `apply_neighbor`.
         * 
         * @param devices The global state vector of devices.
         * @param servers The global state vector of servers.
         * @param move The `Move` structure containing the transition details to undo.
         */
        inline void revert_neighbor(core::Devices& devices, core::Servers& servers, const Move& move) {
            core::Device& device = devices[move.device_id];
                
            if (move.new_server_id != -1) {
                servers[move.new_server_id].rmv_service(device);
            }
            if (move.swapped_id != -1) {
                servers[move.new_server_id].add_service(devices[move.swapped_id]);
            }
            if (move.old_server_id != -1) {
                servers[move.old_server_id].add_service(device);
            }
        }

        /**
         * @brief Executes the Simulated Annealing (SA) optimization algorithm.
         * @details Iteratively explores the solution space by applying random neighborhood moves. 
         * Accepts worsening moves according to the Metropolis criterion \f$\exp(-\Delta / T)\f$ 
         * to escape local optima, cooling down geometrically via factor \f$\alpha\f$ until \f$T \le 10^{-3}\f$.
         * Uses lazy synchronization for tracking the best-found state.
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
            bool is_best_synced = true;
            
            double T = metrics_ptr->temperature;
            double alpha = metrics_ptr->alpha;
            
            auto start_chrono = std::chrono::high_resolution_clock::now();

            while (T > 1e-3) {
                for (int i = 0; i < LOOP; i++) { 
                    Move move = apply_neighbor(state.devices, state.servers);
                    if (!move.valid) continue;
                    
                    double neighbor_cost = current_cost + move.delta_cost;
                    double delta = neighbor_cost - current_cost;

                    if (delta < 0.0 || (randomness::random_number(0.0, 1.0) < std::exp(-delta / T))) {
                        if (delta > 0.0 && !is_best_synced) {
                            revert_neighbor(state.devices, state.servers, move);
                            best_devices = state.devices;
                            best_servers = state.servers;
                            is_best_synced = true;
                            if (move.swapped_id != -1) {
                                state.servers[move.new_server_id].rmv_service(state.devices[move.swapped_id]);
                            }
                            if (move.old_server_id != -1) {
                                state.servers[move.old_server_id].rmv_service(state.devices[move.device_id]);
                            }
                            if (move.new_server_id != -1) {
                                state.servers[move.new_server_id].add_service(state.devices[move.device_id]);
                            }
                        }

                        current_cost = neighbor_cost;

                        if (current_cost < best_cost - ZERO) {
                            i = 0;
                            best_cost = current_cost;
                            is_best_synced = false;
                        }
                    } else {
                        revert_neighbor(state.devices, state.servers, move);
                    }
                }
                T *= alpha;
            }
            
            if (is_best_synced) {
                state.devices = std::move(best_devices);
                state.servers = std::move(best_servers);
            }

            auto end_chrono = std::chrono::high_resolution_clock::now();
            metrics_ptr->outputs.execution_time += std::chrono::duration<double>(end_chrono - start_chrono).count();
        } 
    }

    /**
     * @brief Initializes and launches the selected meta-heuristic algorithm.
     * @details Generates an initial solution using the configured constructive base heuristic, 
     * records its standalone metrics in the heuristic results directory, and then executes 
     * the meta-heuristic optimization over the resulting state.
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
            state.stats
        );

        heuristic::boot(initial_state);
        metrics::calculate_metrics(initial_state);
        if (metrics_ptr->heuristic != metrics::Algorithm::greedy_cost_benefit || metrics_ptr->run_id == 1) {
            metrics::save_results(initial_state, false);
        }

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