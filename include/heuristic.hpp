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
         * @brief Executes a Server-Centric Greedy Resource Allocation based on pseudo-utility.
         * @details Calculates the cost-benefit ratio of unserved devices relative to each server's 
         * specific capacity. Devices are allocated greedily per server to maximize efficiency.
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
           
            for (size_t i = 1; i < servers.size(); i++) {
                core::Server& server = servers[i];

                id_pair_vector candidates;

                for (size_t d = 1; d < devices.size(); d++) {
                    core::Device& device = devices[d];
                    if (!device.served) {
                        double w_cpu = device.cpu / server.cpu;
                        double w_mem = device.mem / server.mem;
                        double w_sto = device.sto / server.sto;

                        double utility = device.cost / (w_cpu + w_mem + w_sto);
                        candidates.push_back(std::make_pair(device.id, utility));
                    }
                }
                
                std::sort(candidates.begin(), candidates.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) {return a.second > b.second;});
                
                for (const auto& candidate : candidates) {
                    core::Device& device = devices[candidate.first];
                    
                    if (server.can_serve(device)) {
                        server.add_service(device);
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
            
            auto start_chrono = std::chrono::high_resolution_clock::now();
           
            for (size_t d = 1; d < devices.size(); d++) {
                core::Device& device = devices[d];

                int i = randomness::random_number<int>(1, static_cast<int>(servers.size() - 1));
                core::Server& server = servers[i];

                if (server.can_serve(device)) {
                    server.add_service(device);
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

                int i = randomness::random_number<int>(1, static_cast<int>(ec_count));
                core::Server& server = servers[i];

                if (server.can_serve(device)) {
                    server.add_service(device);
                } else {
                    i = randomness::random_number<int>(ec_count + 1, static_cast<int>(ec_count + cc_count));
                    core::Server& server = servers[i];
                    
                    if (server.can_serve(device)) {
                        server.add_service(device);
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