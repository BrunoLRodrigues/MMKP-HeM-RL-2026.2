/**
 * @file main.cpp
 * @brief Entry point and benchmark runner for the MMKP IoT-EC-CC resource allocation simulator.
 * @details Defines the default experimental parameters, parses optional command-line overrides
 * passed by external scripts (e.g., Python), and orchestrates the execution across Exact,
 * Heuristic, and Meta-heuristic paradigms.
 */

#include "mmkp.hpp"
#include "exact.hpp"
#include "heuristic.hpp"
#include "metaheuristic.hpp"

/**
 * @struct SimulationConfig
 * @brief Holds the runtime configuration parameters for the simulation benchmark with default fallbacks.
 */
struct SimulationConfig {
    int services = 100;             ///< @brief Total number of distinct service profiles generated.
    int devices_start = 15000;      ///< @brief Initial number of IoT devices in the scaling benchmark.
    int devices_step = 1000;        ///< @brief Increment step for the number of IoT devices per batch.
    int devices_end = 20000;        ///< @brief Maximum number of IoT devices in the scaling benchmark.
    int ec_length = 499;            ///< @brief Total number of Edge Computing (EC) servers available.
    int cc_length = 1;              ///< @brief Total number of Cloud Computing (CC) servers available.
    int max_runs = 50;              ///< @brief Number of independent runs per instance configuration.
    int run_id = 0;                 ///< @brief Specific run/episode ID override (when > 0, executes only this run).
    double cplex_time_limit = 3600; ///< @brief Time limit in seconds for the CPLEX exact solver.
    double temperature = 50.0;      ///< @brief Initial temperature for Simulated Annealing.
    double alpha = 0.9999;          ///< @brief Geometric cooling factor for Simulated Annealing.
    bool fixed_penalty = false;     ///< @brief Whether the penalty (cost) of each device is fixed or can vary.
    int option_start = 0;           ///< @brief First algorithm option identifier in the benchmark range.
    int option_end = 3;             ///< @brief Last algorithm option identifier in the benchmark range.
    bool training = false;          ///< @brief Routes data and results to `training/` subdirectories when enabled.
    bool init_only = false;         ///< @brief Generates/loads the instance and prints RL state features without running solvers.
};

/**
 * @brief Parses command-line arguments and overrides default configuration values.
 * @details Reads flag-value pairs passed via CLI. Any parameter not explicitly provided
 * retains its default value defined in `SimulationConfig`.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return SimulationConfig Populated configuration structure.
 */
SimulationConfig parse_arguments(int argc, char* argv[]) {
    SimulationConfig config;

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string arg = argv[i];
        std::string val = argv[i + 1];

        if (arg == "--services") config.services = std::stoi(val);
        else if (arg == "--devices-start") config.devices_start = std::stoi(val);
        else if (arg == "--devices-step") config.devices_step = std::stoi(val);
        else if (arg == "--devices-end") config.devices_end = std::stoi(val);
        else if (arg == "--ec-length") config.ec_length = std::stoi(val);
        else if (arg == "--cc-length") config.cc_length = std::stoi(val);
        else if (arg == "--max-runs") config.max_runs = std::stoi(val);
        else if (arg == "--cplex-time-limit") config.cplex_time_limit = std::stod(val);
        else if (arg == "--temperature") config.temperature = std::stod(val);
        else if (arg == "--alpha") config.alpha = std::stod(val);
        else if (arg == "--fixed-penalty") config.fixed_penalty = (val == "1" || val == "true");
        else if (arg == "--option-start") config.option_start = std::stoi(val);
        else if (arg == "--option-end") config.option_end = std::stoi(val);
        else if (arg == "--run-id") config.run_id = std::stoi(val);
        else if (arg == "--training") config.training = (val == "1" || val == "true");
        else if (arg == "--init-only") config.init_only = (val == "1" || val == "true");
        else std::cerr << "Warning: Unknown argument " << arg << "\n";
    }

    return config;
}

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
 *               - `-4`: Constructive heuristic (`balanced_greedy_random`)
 *               - `1`: Simulated Annealing + `greedy_cost_benefit`
 *               - `2`: Simulated Annealing + `greedy_random`
 *               - `3`: Simulated Annealing + `greedy_random_fallback`
 *               - `4`: Simulated Annealing + `balanced_greedy_random`
 * @param run_id The current execution run number (1-indexed).
 * @param config Configuration parameters containing solver limits and metaheuristic hyperparameters.
 */
void run_simulation(const metrics::Result& base, int option, int run_id, const SimulationConfig& config = SimulationConfig{}) {
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
    } else if (option == -4) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::balanced_greedy_random;
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
    } else if (option == 4) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::balanced_greedy_random;
    } else if (option == 5) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::timed_simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_cost_benefit;
    } else if (option == 6) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::timed_simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_random;
    } else if (option == 7) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::timed_simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_random_fallback;
    } else if (option == 8) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::timed_simulated_annealing;
        initial_heuristic = metrics::Algorithm::balanced_greedy_random;    
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
        state.stats = std::make_shared<metrics::MetaHeuristicMetrics>(
            algorithm, run_id, base.stats, config.temperature, config.alpha, initial_heuristic
        );
    }

    if (paradigm == metrics::Paradigm::EXACT) {
        exact::boot(state, config.cplex_time_limit);
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
 * @details Parses optional CLI parameters, iterates over the configured range of device counts
 * and independent runs, executing deterministic algorithms once per instance and stochastic
 * algorithms across all runs.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return int Exit status code (`0` on normal termination).
 */
int main(int argc, char* argv[]) {
    const SimulationConfig config = parse_arguments(argc, argv);
    data::generate::is_training_mode() = config.training;

    if (config.training) {
        if (config.init_only) {
            data::generate::resolve_episode(config.run_id);
            const int random_devices = randomness::random_number(config.devices_start, config.devices_end);
            metrics::Result base_state(random_devices, config.services, config.ec_length, config.cc_length, config.fixed_penalty);
            metrics::print_rl_state(base_state);
            return 0;
        }

        const int episodes_to_run = (config.run_id > 0) ? 1 : config.max_runs;

        for (int i = 0; i < episodes_to_run; i++) {
            const int ep_id = data::generate::resolve_episode(config.run_id);
            const int random_devices = randomness::random_number(config.devices_start, config.devices_end);
            metrics::Result base_state(random_devices, config.services, config.ec_length, config.cc_length, config.fixed_penalty);

            for (int option = config.option_start; option <= config.option_end; option++) {
                run_simulation(base_state, option, ep_id, config);
            }
        }
        return 0;
    }

    for (int devices_length = config.devices_start; devices_length <= config.devices_end; devices_length += config.devices_step) {
        metrics::Result base_state(devices_length, config.services, config.ec_length, config.cc_length, config.fixed_penalty);

        const int start_run = (config.run_id > 0) ? config.run_id : 1;
        const int end_run   = (config.run_id > 0) ? config.run_id : config.max_runs;

        for (int run = start_run; run <= end_run; run++) {
            for (int option = config.option_start; option <= config.option_end; option++) {
                if (config.run_id == 0 && run > 1 && (option == 0 || option == -1)) continue;
                run_simulation(base_state, option, run, config);
            }
        }
    }

    return 0;
}
