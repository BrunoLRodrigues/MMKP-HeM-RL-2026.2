#include "mmkp.hpp"
#include "exact.hpp"
#include "heuristic.hpp"
#include "metaheuristic.hpp"

int SERVICES = 100;
int DEVICES_START = 6500; 
int DEVICES_STEP = 100;  
int DEVICES_END = 6900;  
int EC_LENGTH = 99;      
int CC_LENGTH = 1;       
int MAX_RUNS = 1;       

double CPLEX_TIME_LIMIT = 3600; 
double TEMPERATURE = 1000.0;      
double ALPHA = 0.9999;

/**
 * @brief Executes a specific algorithm configuration for the network resource allocation simulation.
 * @param base The initial system state containing untouched devices and servers.
 * @param option The identifier for the algorithm combination to execute.
 * @param run_id The current execution run number.
 */
void run_simulation(const metrics::Result& base, int option, int run_id) {
    metrics::Paradigm paradigm;
    metrics::Algorithm algorithm, initial_heuristic;

    if (option == 0) {
        paradigm = metrics::Paradigm::EXACT;
        algorithm = metrics::Algorithm::solver_minimum_cost;
    } else if (option == 1) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_cost_benefit;
    } else if (option == 2) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_random;
    } else if (option == 3) {
        paradigm = metrics::Paradigm::HEURISTIC;
        algorithm = metrics::Algorithm::greedy_random_fallback;
    } else if (option == 4) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_cost_benefit;
    } else if (option == 5) {
        paradigm = metrics::Paradigm::METAHEURISTIC;
        algorithm = metrics::Algorithm::simulated_annealing;
        initial_heuristic = metrics::Algorithm::greedy_random;
    } else if (option == 6) {
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

int main() {
    for (int devices_length = DEVICES_START; devices_length <= DEVICES_END; devices_length += DEVICES_STEP) {
        metrics::Result base_state(devices_length, SERVICES, EC_LENGTH, CC_LENGTH);

        for (int run = 1; run <= MAX_RUNS; run++) {
            for (int option = 4; option <= 4; option++) {
                if (run > 1 && (option <= 1)) continue;
                run_simulation(base_state, option, run);
            }
        }
    }

    return 0;
}
