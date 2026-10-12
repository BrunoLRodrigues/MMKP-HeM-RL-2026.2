#pragma once

#include "mmkp.hpp"

#include <ilcplex/ilocplex.h>

/**
 * @brief Type alias for a 2D array of CPLEX numerical variables.
 */
typedef IloArray<IloNumVarArray> NumVar2D;

/**
 * @brief Type alias for a 3D array of CPLEX numerical variables.
 */
typedef IloArray<NumVar2D> NumVar3D;

/**
 * @brief Contains mathematical programming models for exact optimization.
 * @details This namespace utilizes the IBM ILOG CPLEX optimizer to solve the network 
 * resource allocation problem to optimality or within a specified time limit, providing 
 * an exact baseline for cost minimization.
 */
namespace exact {
    
    namespace {

        /**
         * @brief Formulates and solves the exact Min-Cost network resource allocation model using CPLEX.
         * @details Sets up decision variables for device assignment. Defines the objective function 
         * to minimize total non-service penalties. Applies constraints to ensure devices are assigned 
         * to at most one server and that server resource capacities (CPU, MEM, STO) are not exceeded. 
         * Parses the optimized variable values back into the simulation state.
         * 
         * @param state The unified simulation result state containing devices, servers, and metrics.
         * @param time_limit_sec The maximum execution time allowed for the CPLEX solver in seconds.
         */
        inline void solver_minimum_cost(metrics::Result& state, int time_limit_sec = 3600) {
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
                
                #pragma region VariableDeclaration
                //=========================================================================
                // 1. VARIABLE DECLARATION
                //=========================================================================

                // w_{d}: 1 if device d is NOT served, 0 otherwise.
                IloNumVarArray w(env, devices.size(), 0, 1, ILOBOOL);
                for (size_t d = 1; d < devices.size(); d++) {
                    std::string name = "w_d(" + std::to_string(d) + ")";
                    w[d].setName(name.c_str());
                }

                // x^{i}_{d}: 1 if device d is allocated to server i, 0 otherwise.
                NumVar2D x(env, servers.size());
                for (size_t i = 1; i < servers.size(); i++) {
                    x[i] = IloNumVarArray(env, devices.size(), 0, 1, ILOBOOL);
                    for (size_t d = 1; d < devices.size(); d++) {
                        std::string name = "x_s(" + std::to_string(i) + ")_d(" + std::to_string(d) + ")";
                        x[i][d].setName(name.c_str());
                    }
                }
                #pragma endregion
                
                #pragma region ObjectiveFunction
                //=========================================================================
                // 2. OBJECTIVE FUNCTION
                // Minimize total cost: non-service penalties only.
                //=========================================================================

                IloExpr obj(env);
                for (size_t d = 1; d < devices.size(); d++) {
                    obj += devices[d].cost * w[d];
                }
                model.add(IloMinimize(env, obj));
                obj.end();
                #pragma endregion

                #pragma region Constraints
                //=========================================================================
                // 3. CONSTRAINTS
                //=========================================================================

                for (size_t d = 1; d < devices.size(); d++) {
                    // Constraint (1): Each device is served by at most one server globally.
                    IloExpr c1(env);
                    for (size_t i = 1; i < servers.size(); i++) {
                        c1 += x[i][d];
                    }
                    model.add(c1 == (1 - w[d]));
                    c1.end();
                }

                // Constraints (2-4): Server resource capacity limits.
                for (size_t i = 1; i < servers.size(); i++) {
                    IloExpr c_cpu(env), c_mem(env), c_sto(env);
                    
                    for (size_t d = 1; d < devices.size(); d++) {
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
                cplex.setParam(IloCplex::Param::Threads, 1);
                cplex.setParam(IloCplex::Param::TimeLimit, time_limit_sec);
                cplex.setParam(IloCplex::Param::MIP::Tolerances::Integrality, ZERO);

                std::filesystem::path run_dir = metrics_ptr->get_base_directory_path();
                std::filesystem::path log_dir = run_dir / "log";
                std::filesystem::path model_dir = run_dir / "model";
                
                std::filesystem::create_directories(log_dir);
                std::filesystem::create_directories(model_dir);
                
                std::string file_name = "Devices" + std::to_string(metrics_ptr->inputs.devices_count) + 
                                        "_EC" + std::to_string(metrics_ptr->inputs.servers_ec_count) + 
                                        "_CC" + std::to_string(metrics_ptr->inputs.servers_cc_count);
                
                std::filesystem::path log_path = log_dir / (file_name + ".log");
                std::filesystem::path model_path = model_dir / (file_name + ".lp");

                std::ofstream log_file(log_path);
                if (log_file.is_open()) {
                    cplex.setOut(log_file);
                } else {
                    cplex.setOut(env.getNullStream());
                }

                cplex.exportModel(model_path.c_str());

                auto start_chrono = std::chrono::high_resolution_clock::now();
                cplex.solve();
                auto end_chrono = std::chrono::high_resolution_clock::now();
                
                metrics_ptr->outputs.execution_time = std::chrono::duration<double>(end_chrono - start_chrono).count();

                std::stringstream status;
                status << cplex.getStatus();
                metrics_ptr->status = status.str();
                
                log_file.close();
                #pragma endregion

                #pragma region Result
                //=========================================================================
                // 5. PARSE RESULTS
                //=========================================================================

                if (cplex.getStatus() == IloAlgorithm::Optimal || cplex.getStatus() == IloAlgorithm::Feasible) {
                    
                    metrics_ptr->objective_function = (double)cplex.getObjValue();
                    metrics_ptr->outputs.total_penalty_cost = metrics_ptr->objective_function;
                    metrics_ptr->gap = cplex.getMIPRelativeGap();

                    for (size_t d = 1; d < devices.size(); d++) {
                        if (cplex.getValue(w[d]) < 0.5) { // Device is served
                            for (size_t i = 1; i < servers.size(); i++) {
                                if (cplex.getValue(x[i][d]) > 0.5) {
                                    servers[i].add_service(devices[d]);
                                    break;
                                }
                            }
                        } 
                    }
                }
                #pragma endregion

            } catch (const IloException& e) {
                std::cerr << "CPLEX Error: " << e.getMessage() << "\n";
            } catch (...) {
                std::cerr << "An unknown error occurred in the CPLEX model.\n";
            }
            env.end();
        }
    }

    /**
     * @brief Bootstraps and executes the configured exact mathematical model.
     * @details Reads the specified algorithm name from the metrics and dispatches 
     * the execution to the corresponding exact solver.
     * 
     * @param state The unified simulation state to be processed.
     * @param time_limit_sec The maximum execution time allowed for the exact solver in seconds.
     */
    inline void boot(metrics::Result& state, int time_limit_sec = 3600) {
        if (!state.stats) return;

        if (metrics::Algorithm::solver_minimum_cost == state.stats->algorithm) {
            solver_minimum_cost(state, time_limit_sec);
        } else {
            std::cerr << "Error: Unknown mathematical model algorithm type.\n";
        }
    }
}