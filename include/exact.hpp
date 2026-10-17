/**
 * @file exact.hpp
 * @brief Exact Integer Linear Programming (ILP) formulations using IBM ILOG CPLEX.
 * @details Provides the mathematical optimization model and solver configuration
 * to solve the Multiple Multidimensional Knapsack Problem (MMKP) for IoT-EC-CC
 * resource allocation.
 */

#pragma once

#include "mmkp.hpp"

#include <ilcplex/ilocplex.h>

/**
 * @namespace exact
 * @brief Contains mathematical programming models for exact optimization.
 * @details Utilizes the IBM ILOG CPLEX optimizer to solve the network resource 
 * allocation problem to optimality or within a specified time limit, providing 
 * an exact baseline by maximizing avoided non-service penalties.
 */
namespace exact {

    /**
     * @brief Type alias for a 2D matrix of CPLEX decision variables.
     */
    using NumVar2D = IloArray<IloNumVarArray>;

    namespace {

        /**
         * @brief Formulates and solves the exact ILP resource allocation model using CPLEX.
         * @details Defines binary decision variables \f$x_{d}^{i} \in \{0, 1\}\f$ indicating whether 
         * device \f$d\f$ is assigned to server \f$i\f$. Maximizes the total utility of served devices 
         * (avoided SLA penalties) subject to single-server assignment and multidimensional 
         * resource capacity constraints (CPU, MEM, STO).
         * 
         * @param state The unified simulation state containing devices, servers, and metrics.
         * @param time_limit_sec The maximum execution time allowed for the solver in seconds.
         */
        inline void solver_maximum_cost(metrics::Result& state, int time_limit_sec = 3600) {
            core::Devices& devices = state.devices;
            core::Servers& servers = state.servers;
            
            auto metrics_ptr = std::dynamic_pointer_cast<metrics::ExactMetrics>(state.stats);
            if (!metrics_ptr) {
                std::cerr << "Error: Invalid metrics type for exact solver.\n";
                return;
            }

            IloEnv env;
            try {
                IloModel model(env);
                const size_t num_devices = devices.size();
                const size_t num_servers = servers.size();
                
                #pragma region VariableDeclaration
                //=========================================================================
                // 1. VARIABLE DECLARATION
                //=========================================================================

                // x^{i}_{d}: 1 if device d is allocated to server i, 0 otherwise.
                NumVar2D x(env, num_servers);
                for (size_t i = 1; i < num_servers; i++) {
                    x[i] = IloNumVarArray(env, num_devices, 0, 1, ILOBOOL);
                }
                #pragma endregion
                
                #pragma region ObjectiveFunctionAndConstraints
                IloExpr obj(env);

                for (size_t d = 1; d < num_devices; d++) {
                    IloExpr c1(env);
                    for (size_t i = 1; i < num_servers; i++) {
                        c1 += x[i][d];
                        obj += devices[d].cost * x[i][d];
                    }
                    model.add(c1 <= 1);
                    c1.end();
                }

                model.add(IloMaximize(env, obj));
                obj.end();

                for (size_t i = 1; i < num_servers; i++) {
                    IloExpr c_cpu(env), c_mem(env), c_sto(env);
                    
                    for (size_t d = 1; d < num_devices; d++) {
                        c_cpu += devices[d].cpu * x[i][d]; 
                        c_mem += devices[d].mem * x[i][d]; 
                        c_sto += devices[d].sto * x[i][d]; 
                    }

                    model.add(c_cpu <= servers[i].cpu); 
                    model.add(c_mem <= servers[i].mem); 
                    model.add(c_sto <= servers[i].sto); 
                    
                    c_cpu.end(); 
                    c_mem.end(); 
                    c_sto.end();
                }              
                #pragma endregion

                #pragma region SolverConfiguration
                //=========================================================================
                // 4. SOLVER CONFIGURATION AND EXECUTION
                //=========================================================================

                IloCplex cplex(model);
                cplex.setOut(env.getNullStream());
                cplex.setWarning(env.getNullStream());
                
                cplex.setParam(IloCplex::Param::Threads, 1);
                cplex.setParam(IloCplex::Param::ClockType, 2);
                cplex.setParam(IloCplex::Param::TimeLimit, time_limit_sec);

                cplex.setParam(IloCplex::Param::WorkMem, 16384);
                cplex.setParam(IloCplex::Param::MIP::Strategy::File, 3);

                // Relative MIP gap tolerance of 1e-3 (0.001)
                cplex.setParam(IloCplex::Param::MIP::Tolerances::MIPGap, 1e-3);
                cplex.setParam(IloCplex::Param::Emphasis::MIP, IloCplex::MIPEmphasisBalanced);

                auto start_chrono = std::chrono::high_resolution_clock::now();
                const IloBool has_solution = cplex.solve();
                auto end_chrono = std::chrono::high_resolution_clock::now();
                
                metrics_ptr->outputs.execution_time = std::chrono::duration<double>(end_chrono - start_chrono).count();

                std::stringstream status;
                status << cplex.getStatus();
                metrics_ptr->status = status.str();
                #pragma endregion

                #pragma region Result
                //=========================================================================
                // 5. PARSE RESULTS
                //=========================================================================

                if (has_solution) {
                    metrics_ptr->objective_function = static_cast<double>(cplex.getObjValue());
                    metrics_ptr->gap = static_cast<double>(cplex.getMIPRelativeGap());

                    for (size_t i = 1; i < num_servers; i++) {
                        for (size_t d = 1; d < num_devices; d++) {
                            if (cplex.isExtracted(x[i][d]) && cplex.getValue(x[i][d]) > 0.5) {
                                servers[i].add_service(devices[d]);
                            }
                        }
                    }
                }

            } catch (const IloException& e) {
                std::cerr << "CPLEX Error: " << e.getMessage() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "Standard Exception: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "An unknown error occurred in the CPLEX model.\n";
            }
            env.end();
        }
    }

    /**
     * @brief Bootstraps and executes the configured exact mathematical model.
     * @details Reads the specified algorithm identifier from `state.stats` and dispatches 
     * execution to the corresponding exact solver implementation.
     * 
     * @param state The unified simulation state to be processed.
     * @param time_limit_sec The maximum execution time allowed for the exact solver in seconds.
     */
    inline void boot(metrics::Result& state, int time_limit_sec = 3600) {
        if (!state.stats) return;

        if (metrics::Algorithm::solver_maximum_cost == state.stats->algorithm) {
            solver_maximum_cost(state, time_limit_sec);
        } else {
            std::cerr << "Error: Unknown mathematical model algorithm type.\n";
        }
    }
}