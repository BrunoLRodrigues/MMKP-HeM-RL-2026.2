/**
 * @file main.cpp
 * @brief Entry point and benchmark runner for the MMKP IoT-EC-CC resource allocation simulator.
 * @details Defines the experimental parameters (scaling of IoT devices, server counts, 
 * CPLEX time limit, and Simulated Annealing cooling schedule) and orchestrates 
 * the execution across Exact, Heuristic, and Meta-heuristic paradigms.
 */

#include "mmkp.hpp"
#include "exact.hpp"
#include "heuristic.hpp"
#include "metaheuristic.hpp"

constexpr int SERVICES = 100;             ///< @brief Total number of distinct service profiles generated.
constexpr int DEVICES_START = 10000;      ///< @brief Initial number of IoT devices in the scaling benchmark.
constexpr int DEVICES_STEP = 1000;        ///< @brief Increment step for the number of IoT devices per batch.
constexpr int DEVICES_END = 20000;        ///< @brief Maximum number of IoT devices in the scaling benchmark.
constexpr int EC_LENGTH = 499;            ///< @brief Total number of Edge Computing (EC) servers available.
constexpr int CC_LENGTH = 1;              ///< @brief Total number of Cloud Computing (CC) servers available.
constexpr int MAX_RUNS = 50;              ///< @brief Number of independent runs per instance configuration.
constexpr double CPLEX_TIME_LIMIT = 3600; ///< @brief Time limit in seconds for the CPLEX exact solver.
constexpr double TEMPERATURE = 50.0;      ///< @brief Initial temperature for Simulated Annealing.
constexpr double ALPHA = 0.9999;          ///< @brief Geometric cooling factor for Simulated Annealing.

/**
 * @brief Executes a specific algorithm configuration for the network resource allocation simulation.
 * @details Instantiates the appropriate metrics tracker based on the selected option, dispatches
 * the simulation state to the corresponding solver paradigm (Exact, Heuristic, or Meta-heuristic),
 * computes final utilization/cost metrics, and saves the results to disk.
 * 
 * @param base The initial system state containing untouched devices and servers.
 * @param option The identifier for the algorithm combination to execute:
 *               - `0`: Exact CPLEX solver (`solver_maximum_cost`)
 *               - `-1`: Constructive heuristic (`greedy_cost_benefit`)
 *               - `-2`: Constructive heuristic (`greedy_random`)
 *               - `-3`: Constructive heuristic (`greedy_random_fallback`)
 *               - `1`: Simulated Annealing + `greedy_cost_benefit`
 *               - `2`: Simulated Annealing + `greedy_random`
 *               - `3`: Simulated Annealing + `greedy_random_fallback`
 * @param run_id The current execution run number (1-indexed).
 */
void run_simulation(const metrics::Result& base, int option, int run_id) {
    metrics::Paradigm paradigm = metrics::Paradigm::NONE;
    metrics::Algorithm algorithm = metrics::Algorithm::NONE;
    metrics::Algorithm initial_heuristic = metrics::Algorithm::NONE;

    if (option == 0) {
        paradigm = metrics::Paradigm::EXACT;
        algorithm = metrics::Algorithm::solver_maximum_cost;
    } else if (option == -1) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_cost_benefit;
    } else if (option == -2) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_random;
    } else if (option == -3) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_random_fallback;
    } else if (option == 1) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_cost_benefit;
    } else if (option == 2) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_random;
    } else if (option == 3) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_random_fallback;
    } else {
        std::cerr << "Error: Invalid option selected for simulation.\n";
        return;
    }

    metrics::Result state = base;

    if (paradigm == metrics::Paradigm::EXACT) {
        state.stats = std::make_shared<metrics::ExactMetrics>(algorithm, run_id, base.stats);
    } else if (paradigm == metrics::Paradigm::HEURISTIC) {
        state.stats = std::make_shared<metrics::HeuristicMetrics>(algorithm, run_id, base.stats);
    } else if (paradigm == metrics::Paradigm::METAHEURISTIC) {
        state.stats = std::make_shared<metrics::MetaHeuristicMetrics>(algorithm, run_id, base.stats, TEMPERATURE, ALPHA, initial_heuristic);
    }

    if (paradigm == metrics::Paradigm::EXACT) {
        exact::boot(state, CPLEX_TIME_LIMIT);
    } else if (paradigm == metrics::Paradigm::HEURISTIC) {
        heuristic::boot(state);
    } else if (paradigm == metrics::Paradigm::METAHEURISTIC) {
        metaheuristic::boot(state);
    }

    metrics::calculate_metrics(state);
    metrics::save_results(state, false);
}

/**
 * @brief Main entry point for the network resource allocation simulation benchmark.
 * @details Iterates over the configured range of device counts and independent runs,
 * executing deterministic algorithms once per instance and stochastic algorithms across
 * all runs.
 * 
 * @return int Exit status code (`0` on normal termination).
 */
int main() {
    for (int devices_length = DEVICES_START; devices_length <= DEVICES_END; devices_length += DEVICES_STEP) {
        metrics::Result base_state(devices_length, SERVICES, EC_LENGTH, CC_LENGTH);

        for (int run = 1; run <= MAX_RUNS; run++) {
            for (int option = 0; option <= 3; option++) {
                if (run > 1 && (option == 0 || option == -1)) continue;
                run_simulation(base_state, option, run);
            }
        }
    }

    return 0;
}
