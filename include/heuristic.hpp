#pragma once

#include "mmkp.hpp"

/**
 * @brief Contains heuristic algorithms for network resource allocation.
 * @details Implements fast, suboptimal solution strategies (like Greedy, GRASP) 
 * prioritizing low execution times while attempting to minimize device penalty costs.
 */
namespace heuristic {

    namespace {

        /**
         * @brief Executes a Global Greedy Resource Allocation based on surrogate pseudo-utility.
         * @details Normalizes device resource demands against total system capacity to compute 
         * pseudo-utility and sorts candidates once globally. Allocates devices greedily per server 
         * using prefix-skipping and minimum-demand early termination.
         * 
         * @param state The unified simulation result state containing devices, servers, and metrics.
         */
        inline void greedy_cost_benefit(metrics::Result& state) {
            auto metrics_ptr = std::dynamic_pointer_cast<metrics::HeuristicMetrics>(state.stats);
            if (!metrics_ptr) {
                std::cerr << "Error: Invalid metrics type for heuristic solver.\n";
                return;
            }

            core::Devices& devices = state.devices;
            core::Servers& servers = state.servers;

            auto start_chrono = std::chrono::high_resolution_clock::now();

            double total_cpu = 0.0, total_mem = 0.0, total_sto = 0.0;
            for (size_t i = 1; i < servers.size(); i++) {
                total_cpu += servers[i].cpu;
                total_mem += servers[i].mem;
                total_sto += servers[i].sto;
            }

            id_pair_vector candidates;
            candidates.reserve(devices.size());

            double min_cpu = std::numeric_limits<double>::max();
            double min_mem = std::numeric_limits<double>::max();
            double min_sto = std::numeric_limits<double>::max();

            for (size_t d = 1; d < devices.size(); d++) {
                core::Device& device = devices[d];
                if (!device.served) {
                    double w_cpu = device.cpu / total_cpu;
                    double w_mem = device.mem / total_mem;
                    double w_sto = device.sto / total_sto;

                    double utility = device.cost / (w_cpu + w_mem + w_sto);
                    candidates.emplace_back(device.id, utility);

                    if (device.cpu < min_cpu) min_cpu = device.cpu;
                    if (device.mem < min_mem) min_mem = device.mem;
                    if (device.sto < min_sto) min_sto = device.sto;
                }
            }

            std::sort(candidates.begin(), candidates.end(), [](const id_pair& a, const id_pair& b) {
                return a.second > b.second;
            });

            size_t start_idx = 0;
            const size_t num_candidates = candidates.size();

            for (size_t i = 1; i < servers.size() && start_idx < num_candidates; i++) {
                core::Server& server = servers[i];

                for (size_t c = start_idx; c < num_candidates; c++) {
                    if ((server.cpu - server.supply.cpu < min_cpu) ||
                        (server.mem - server.supply.mem < min_mem) ||
                        (server.sto - server.supply.sto < min_sto)) {
                        break;
                    }

                    core::Device& device = devices[candidates[c].first];
                    if (!device.served && server.can_serve(device)) {
                        server.add_service(device);
                    }

                    if (c == start_idx && device.served) {
                        start_idx++;
                    }
                }
            }

            auto end_chrono = std::chrono::high_resolution_clock::now();
            metrics_ptr->outputs.execution_time = std::chrono::duration<double>(end_chrono - start_chrono).count();
        }
    
        /**
         * @brief Executes a fast random baseline resource allocation.
         * @details Iterates through all devices and attempts to assign each one to a 
         * single randomly selected server. If the selected server lacks capacity, 
         * the device remains unserved. Metric calculation is deferred.
         * 
         * @param state The unified simulation result state containing devices, servers, and metrics.
         */
        inline void greedy_random(metrics::Result& state) {
            auto metrics_ptr = std::dynamic_pointer_cast<metrics::HeuristicMetrics>(state.stats);
            if (!metrics_ptr) {
                std::cerr << "Error: Invalid metrics type for heuristic solver.\n";
                return;
            }

            core::Devices& devices = state.devices;
            core::Servers& servers = state.servers;
            
            int size_servers = static_cast<int>(servers.size()) - 1;

            auto start_chrono = std::chrono::high_resolution_clock::now();
           
            for (size_t d = 1; d < devices.size(); d++) {
                core::Device& device = devices[d];

                if (size_servers > 1) {
                    int ec_id = randomness::random_number<int>(1, size_servers);
                    if (servers[ec_id].can_serve(device)) {
                        servers[ec_id].add_service(device);
                    }
                }
            }

            auto end_chrono = std::chrono::high_resolution_clock::now();
            metrics_ptr->outputs.execution_time = std::chrono::duration<double>(end_chrono - start_chrono).count();
        }

        /**
         * @brief Executes a two-stage random fallback resource allocation.
         * @details Attempts to assign each device to a randomly selected Edge server first. 
         * If the Edge server lacks capacity, falls back to a randomly selected Cloud server.
         * 
         * @param state The unified simulation result state containing devices, servers, and metrics.
         */
        inline void greedy_random_fallback(metrics::Result& state) {
            auto metrics_ptr = std::dynamic_pointer_cast<metrics::HeuristicMetrics>(state.stats);
            if (!metrics_ptr) {
                std::cerr << "Error: Invalid metrics type for heuristic solver.\n";
                return;
            }

            int ec_count = metrics_ptr->inputs.servers_ec_count;
            int cc_count = metrics_ptr->inputs.servers_cc_count;

            core::Devices& devices = state.devices;
            core::Servers& servers = state.servers;

            auto start_chrono = std::chrono::high_resolution_clock::now();
            
            for (size_t d = 1; d < devices.size(); d++) {
                core::Device& device = devices[d];

                if (ec_count > 0) {
                    int ec_id = randomness::random_number<int>(1, ec_count);
                    if (servers[ec_id].can_serve(device)) {
                        servers[ec_id].add_service(device);
                        continue;
                    }
                }

                if (cc_count > 0) {
                    int cc_id = randomness::random_number<int>(ec_count + 1, ec_count + cc_count);
                    if (servers[cc_id].can_serve(device)) {
                        servers[cc_id].add_service(device);
                    }
                }
            }

            auto end_chrono = std::chrono::high_resolution_clock::now();
            metrics_ptr->outputs.execution_time = std::chrono::duration<double>(end_chrono - start_chrono).count();
        }
    }

    /**
     * @brief Bootstraps and executes the configured heuristic mathematical model.
     * @details Reads the specified algorithm name from the metrics and dispatches 
     * the execution to the corresponding heuristic solver.
     * 
     * @param state The unified simulation state to be processed.
     */
    inline void boot(metrics::Result& state) {
        if (!state.stats) return;

        if (metrics::Algorithm::greedy_cost_benefit == state.stats->algorithm) {
            greedy_cost_benefit(state);
        } else if (metrics::Algorithm::greedy_random == state.stats->algorithm) {
            greedy_random(state);
        } else if (metrics::Algorithm::greedy_random_fallback == state.stats->algorithm) {
            greedy_random_fallback(state);
        } else {
            std::cerr << "Error: Unknown heuristic algorithm type.\n";
        }
    }
}