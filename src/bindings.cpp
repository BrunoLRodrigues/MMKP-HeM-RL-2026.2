/**
 * @file bindings.cpp
 * @brief Pybind11 module exposing the MMKP simulator directly to Python in-memory.
 * @details Provides persistent episode and evaluation scenario caches, heuristic snapshot
 * serialization, and batch execution helpers for Gymnasium Reinforcement Learning environments.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <unordered_map>

#include "mmkp.hpp"
#include "heuristic.hpp"
#include "metaheuristic.hpp"

namespace py = pybind11;

/**
 * @struct HeuristicSnapshot
 * @brief Stores a compact representation of an initial heuristic allocation and its output metrics.
 */
struct HeuristicSnapshot {
    std::vector<int> device_to_server; ///< @brief Assigned server ID per device (`-1` if unserved).
    metrics::Metrics::Outputs outputs; ///< @brief Calculated metrics from the constructive heuristic run.
};

namespace {
    std::unordered_map<int, std::shared_ptr<metrics::Result>> g_episode_base_cache;
    std::unordered_map<int, HeuristicSnapshot> g_gcb_cache;
    std::unordered_map<int, std::vector<HeuristicSnapshot>> g_bgr_cache;

    std::shared_ptr<metrics::Result> g_eval_base_state;
    HeuristicSnapshot g_eval_gcb_snap;
    std::vector<HeuristicSnapshot> g_eval_bgr_snaps;

    /**
     * @brief Extracts a compact allocation and metrics snapshot from a simulation state.
     * @param state Completed simulation state after running a constructive heuristic.
     * @return HeuristicSnapshot Captured device assignments and output metrics.
     */
    inline HeuristicSnapshot extract_snapshot(const metrics::Result& state) {
        HeuristicSnapshot snap;
        snap.device_to_server.resize(state.devices.size(), -1);
        for (size_t d = 1; d < state.devices.size(); ++d) {
            snap.device_to_server[d] = state.devices[d].server_id;
        }
        snap.outputs = state.stats->outputs;
        return snap;
    }

    /**
     * @brief Applies a saved heuristic snapshot onto a fresh simulation state.
     * @param state Target simulation state to populate with initial server assignments.
     * @param snap Snapshot containing device-to-server mappings and heuristic outputs.
     */
    inline void restore_snapshot(metrics::Result& state, const HeuristicSnapshot& snap) {
        for (size_t d = 1; d < state.devices.size(); ++d) {
            int srv_id = snap.device_to_server[d];
            if (srv_id != -1) {
                state.servers[srv_id].add_service(state.devices[d]);
            }
        }
        state.stats->outputs = snap.outputs;
    }

    /**
     * @brief Writes a single heuristic snapshot in binary format to an output stream.
     * @param out Target binary output file stream.
     * @param snap Snapshot to serialize.
     */
    inline void write_single_snapshot(std::ofstream& out, const HeuristicSnapshot& snap) {
        const size_t vec_size = snap.device_to_server.size();
        out.write(reinterpret_cast<const char*>(&vec_size), sizeof(vec_size));
        out.write(reinterpret_cast<const char*>(snap.device_to_server.data()), vec_size * sizeof(int));
        out.write(reinterpret_cast<const char*>(&snap.outputs), sizeof(metrics::Metrics::Outputs));
    }

    /**
     * @brief Reads a single heuristic snapshot in binary format from an input stream.
     * @param in Source binary input file stream.
     * @param snap Destination snapshot structure.
     * @param expected_vec_size Expected size of the device assignment vector.
     * @return `true` if deserialization succeeded and matched the expected size; `false` otherwise.
     */
    inline bool read_single_snapshot(std::ifstream& in, HeuristicSnapshot& snap, size_t expected_vec_size) {
        size_t vec_size = 0;
        if (!in.read(reinterpret_cast<char*>(&vec_size), sizeof(vec_size))) return false;
        if (vec_size != expected_vec_size) return false;

        snap.device_to_server.resize(vec_size);
        if (!in.read(reinterpret_cast<char*>(snap.device_to_server.data()), vec_size * sizeof(int))) return false;
        if (!in.read(reinterpret_cast<char*>(&snap.outputs), sizeof(metrics::Metrics::Outputs))) return false;
        return true;
    }

    /**
     * @brief Persists the GCB snapshot and all BGR run snapshots to a binary cache file.
     * @param file_path Destination binary cache path.
     * @param gcb_snap Deterministic GCB initial solution snapshot.
     * @param bgr_snaps Vector of stochastic BGR initial solution snapshots.
     * @return `true` if the cache file was written cleanly; `false` otherwise.
     */
    inline bool save_snapshots_to_disk(
        const std::filesystem::path& file_path,
        const HeuristicSnapshot& gcb_snap,
        const std::vector<HeuristicSnapshot>& bgr_snaps
    ) {
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        const size_t num_runs = bgr_snaps.size();
        out.write(reinterpret_cast<const char*>(&num_runs), sizeof(num_runs));
        write_single_snapshot(out, gcb_snap);
        for (const auto& snap : bgr_snaps) {
            write_single_snapshot(out, snap);
        }
        return out.good();
    }

    /**
     * @brief Loads cached GCB and BGR initial solution snapshots from disk if available.
     * @param file_path Source binary cache path.
     * @param gcb_snap Output GCB snapshot.
     * @param bgr_snaps Output vector of BGR snapshots.
     * @param expected_devices_vec_size Expected device vector size for validation.
     * @param expected_num_runs Expected number of BGR runs for validation.
     * @return `true` if valid snapshots were loaded; `false` otherwise.
     */
    inline bool load_snapshots_from_disk(
        const std::filesystem::path& file_path,
        HeuristicSnapshot& gcb_snap,
        std::vector<HeuristicSnapshot>& bgr_snaps,
        size_t expected_devices_vec_size,
        int expected_num_runs
    ) {
        if (!std::filesystem::exists(file_path)) return false;

        std::ifstream in(file_path, std::ios::binary);
        if (!in.is_open()) return false;

        size_t num_runs = 0;
        if (!in.read(reinterpret_cast<char*>(&num_runs), sizeof(num_runs))) return false;
        if (static_cast<int>(num_runs) != expected_num_runs) return false;

        if (!read_single_snapshot(in, gcb_snap, expected_devices_vec_size)) return false;

        bgr_snaps.resize(num_runs);
        for (size_t i = 0; i < num_runs; ++i) {
            if (!read_single_snapshot(in, bgr_snaps[i], expected_devices_vec_size)) return false;
        }
        return true;
    }
}

/**
 * @class PyMMKPRunner
 * @brief Persistent in-memory simulation wrapper for Gymnasium RL environments.
 */
class PyMMKPRunner {
private:
    std::shared_ptr<metrics::Result> base_state_;
    int active_episode_ = 1;
    int active_option_ = 1;
    bool evaluation_mode_ = false;

public:
    /**
     * @brief Constructs a PyMMKPRunner instance.
     */
    PyMMKPRunner() = default;

    /**
     * @brief Prepares a training episode instance and generates baseline heuristic snapshots (1x GCB and N BGR runs).
     * @param episode_id Target 1-based episode index.
     * @param devices_min Minimum number of IoT devices when generating a new episode.
     * @param devices_max Maximum number of IoT devices when generating a new episode.
     * @param services Number of service profiles.
     * @param ec Number of Edge Computing servers.
     * @param cc Number of Cloud Computing servers.
     * @param fixed_penalty Whether device penalty costs are fixed (`1.0`) or variable.
     * @param num_runs Number of BGR runs to generate and cache per episode.
     */
    void prepare_episode(
        int episode_id,
        int devices_min,
        int devices_max,
        int services,
        int ec,
        int cc,
        bool fixed_penalty,
        int num_runs = 30
    ) {
        evaluation_mode_ = false;
        data::generate::is_training_mode() = true;
        data::generate::resolve_episode(episode_id);

        if (g_episode_base_cache.find(episode_id) == g_episode_base_cache.end()) {
            const int random_devices = randomness::random_number(devices_min, devices_max);
            g_episode_base_cache[episode_id] = std::make_shared<metrics::Result>(
                random_devices, services, ec, cc, fixed_penalty
            );
        }

        auto base = g_episode_base_cache[episode_id];

        if (g_gcb_cache.find(episode_id) != g_gcb_cache.end() &&
            g_bgr_cache.find(episode_id) != g_bgr_cache.end() &&
            static_cast<int>(g_bgr_cache[episode_id].size()) == num_runs) {
            return;
        }

        std::filesystem::path ep_results_dir = base->stats->get_base_directory_path();
        std::filesystem::path snap_path = ep_results_dir / "initial_snapshots.bin";
        std::filesystem::path heur_file = ep_results_dir / "heuristics.txt";

        if (std::filesystem::exists(heur_file) &&
            load_snapshots_from_disk(snap_path, g_gcb_cache[episode_id], g_bgr_cache[episode_id], base->devices.size(), num_runs)) {
            return;
        }

        if (std::filesystem::exists(heur_file)) {
            std::filesystem::remove(heur_file);
        }

        {
            metrics::Result gcb_state = *base;
            gcb_state.stats = std::make_shared<metrics::HeuristicMetrics>(
                metrics::Algorithm::greedy_cost_benefit,
                1,
                base->stats
            );
            heuristic::boot(gcb_state);
            metrics::calculate_metrics(gcb_state);
            gcb_state.stats->save_to_file();
            g_gcb_cache[episode_id] = extract_snapshot(gcb_state);
        }

        {
            std::vector<HeuristicSnapshot> bgr_runs;
            bgr_runs.reserve(num_runs);

            for (int run = 1; run <= num_runs; ++run) {
                metrics::Result bgr_state = *base;
                bgr_state.stats = std::make_shared<metrics::HeuristicMetrics>(
                    metrics::Algorithm::balanced_greedy_random,
                    run,
                    base->stats
                );
                heuristic::boot(bgr_state);
                metrics::calculate_metrics(bgr_state);
                bgr_state.stats->save_to_file();
                bgr_runs.push_back(extract_snapshot(bgr_state));
            }
            g_bgr_cache[episode_id] = std::move(bgr_runs);
        }

        save_snapshots_to_disk(snap_path, g_gcb_cache[episode_id], g_bgr_cache[episode_id]);
    }

    /**
     * @brief Loads or creates a benchmark evaluation scenario and prepares cached GCB and BGR initial snapshots.
     * @param devices_count Number of IoT devices in the evaluation instance.
     * @param services Number of service profiles.
     * @param ec Number of Edge Computing servers.
     * @param cc Number of Cloud Computing servers.
     * @param fixed_penalty Whether device penalty costs are fixed (`1.0`) or variable.
     * @param num_runs Number of BGR runs to generate or load.
     * @return std::vector<double> Vector containing the 16 dimensionless RL state features.
     */
    std::vector<double> prepare_evaluation_scenario(
        int devices_count,
        int services,
        int ec,
        int cc,
        bool fixed_penalty,
        int num_runs = 30
    ) {
        evaluation_mode_ = true;
        data::generate::is_training_mode() = false;

        g_eval_base_state = std::make_shared<metrics::Result>(
            devices_count, services, ec, cc, fixed_penalty
        );
        base_state_ = g_eval_base_state;
        active_episode_ = 1;

        const int actual_dev = g_eval_base_state->stats->inputs.devices_count;
        const int actual_ec  = g_eval_base_state->stats->inputs.servers_ec_count;
        const int actual_cc  = g_eval_base_state->stats->inputs.servers_cc_count;

        std::filesystem::path heur_dir = metrics::RESULTS_PATH / "HEURISTIC";
        std::filesystem::path heur_file = heur_dir / "heuristics.txt";
        std::filesystem::path snap_path = heur_dir / "cache" / (
            "Snapshots_D" + std::to_string(actual_dev) +
            "_EC" + std::to_string(actual_ec) +
            "_CC" + std::to_string(actual_cc) +
            "_R" + std::to_string(num_runs) + ".bin"
        );

        if (std::filesystem::exists(heur_file) &&
            load_snapshots_from_disk(snap_path, g_eval_gcb_snap, g_eval_bgr_snaps, g_eval_base_state->devices.size(), num_runs)) {
            return metrics::calculate_rl_state(*base_state_);
        }

        {
            metrics::Result gcb_state = *g_eval_base_state;
            gcb_state.stats = std::make_shared<metrics::HeuristicMetrics>(
                metrics::Algorithm::greedy_cost_benefit,
                1,
                g_eval_base_state->stats
            );
            heuristic::boot(gcb_state);
            metrics::calculate_metrics(gcb_state);
            gcb_state.stats->save_to_file();
            g_eval_gcb_snap = extract_snapshot(gcb_state);
        }

        {
            g_eval_bgr_snaps.clear();
            g_eval_bgr_snaps.reserve(num_runs);

            for (int run = 1; run <= num_runs; ++run) {
                metrics::Result bgr_state = *g_eval_base_state;
                bgr_state.stats = std::make_shared<metrics::HeuristicMetrics>(
                    metrics::Algorithm::balanced_greedy_random,
                    run,
                    g_eval_base_state->stats
                );
                heuristic::boot(bgr_state);
                metrics::calculate_metrics(bgr_state);
                bgr_state.stats->save_to_file();
                g_eval_bgr_snaps.push_back(extract_snapshot(bgr_state));
            }
        }

        save_snapshots_to_disk(snap_path, g_eval_gcb_snap, g_eval_bgr_snaps);
        return metrics::calculate_rl_state(*base_state_);
    }

    /**
     * @brief Activates the prepared evaluation scenario and returns its 16 RL state features.
     * @param option Heuristic option identifier (`5` for GCB, `8` for BGR).
     * @return std::vector<double> Vector containing the 16 dimensionless RL state features.
     */
    std::vector<double> reset_evaluation(int option = 5) {
        if (!g_eval_base_state) {
            throw std::runtime_error("Error: prepare_evaluation_scenario() must be called before reset_evaluation().");
        }
        evaluation_mode_ = true;
        data::generate::is_training_mode() = false;
        active_option_ = option;
        base_state_ = g_eval_base_state;

        return metrics::calculate_rl_state(*base_state_);
    }

    /**
     * @brief Selects or prepares a training episode and returns its 16 RL state features.
     * @param devices_min Minimum number of IoT devices when generating a new episode.
     * @param devices_max Maximum number of IoT devices when generating a new episode.
     * @param services Number of service profiles.
     * @param ec Number of Edge Computing servers.
     * @param cc Number of Cloud Computing servers.
     * @param fixed_penalty Whether device penalty costs are fixed (`1.0`) or variable.
     * @param requested_episode Target 1-based episode index.
     * @param option Heuristic option identifier (`5` for GCB, `8` for BGR).
     * @param num_runs Number of BGR runs per episode.
     * @return std::vector<double> Vector containing the 16 dimensionless RL state features.
     */
    std::vector<double> reset_instance(
        int devices_min,
        int devices_max,
        int services,
        int ec,
        int cc,
        bool fixed_penalty,
        int requested_episode = 1,
        int option = 5,
        int num_runs = 30
    ) {
        evaluation_mode_ = false;
        data::generate::is_training_mode() = true;
        const int ep_id = data::generate::resolve_episode(requested_episode);
        prepare_episode(ep_id, devices_min, devices_max, services, ec, cc, fixed_penalty, num_runs);

        active_episode_ = ep_id;
        active_option_ = option;
        base_state_ = g_episode_base_cache.at(ep_id);

        return metrics::calculate_rl_state(*base_state_);
    }

    /**
     * @brief Executes multiple runs of Timed Simulated Annealing using cached GCB or BGR initial states.
     * @param option Heuristic option identifier (`5` for GCB, `8` for BGR).
     * @param temperature Initial Simulated Annealing temperature \f$T_0\f$.
     * @param alpha Geometric cooling factor \f$\alpha\f$.
     * @param num_runs Number of independent TSA runs to execute.
     * @param model_type RL model label recorded in the `Model_Type` column.
     * @return std::tuple<double, double, double> Tuple of `(average_total_cost, max_penalty_cost, average_execution_time)`.
     */
    std::tuple<double, double, double> step_tsa(
        int option,
        double temperature,
        double alpha,
        int num_runs = 30,
        const std::string& model_type = "NONE"
    ) {
        if (!base_state_) {
            throw std::runtime_error("Error: reset_instance() or prepare_evaluation_scenario() must be called before step_tsa().");
        }

        if (evaluation_mode_) {
            data::generate::is_training_mode() = false;
        } else {
            data::generate::is_training_mode() = true;
            data::generate::resolve_episode(active_episode_);
        }

        metrics::Algorithm meta_algorithm = metrics::Algorithm::NONE;
        metrics::Algorithm initial_heuristic = metrics::Algorithm::NONE;

        if (option == 1) {
            meta_algorithm = metrics::Algorithm::simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_cost_benefit;
        } else if (option == 2) {
            meta_algorithm = metrics::Algorithm::simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_random;
        } else if (option == 3) {
            meta_algorithm = metrics::Algorithm::simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_random_fallback;
        } else if (option == 4) {
            meta_algorithm = metrics::Algorithm::simulated_annealing;
            initial_heuristic = metrics::Algorithm::balanced_greedy_random;
        } else if (option == 5) {
            meta_algorithm = metrics::Algorithm::timed_simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_cost_benefit;
        } else if (option == 6) {
            meta_algorithm = metrics::Algorithm::timed_simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_random;
        } else if (option == 7) {
            meta_algorithm = metrics::Algorithm::timed_simulated_annealing;
            initial_heuristic = metrics::Algorithm::greedy_random_fallback;
        } else if (option == 8) {
            meta_algorithm = metrics::Algorithm::timed_simulated_annealing;
            initial_heuristic = metrics::Algorithm::balanced_greedy_random;
        }

        double sum_total_cost = 0.0;
        double max_cost_val = 0.0;
        double sum_exec_time = 0.0;

        for (int run = 1; run <= num_runs; ++run) {
            metrics::Result state = *base_state_;
            state.stats = std::make_shared<metrics::MetaHeuristicMetrics>(
                meta_algorithm,
                run,
                base_state_->stats,
                temperature,
                alpha,
                initial_heuristic,
                model_type
            );

            bool already_started = false;
            if (initial_heuristic == metrics::Algorithm::greedy_cost_benefit) {
                const auto& snap = evaluation_mode_ ? g_eval_gcb_snap : g_gcb_cache.at(active_episode_);
                restore_snapshot(state, snap);
                already_started = true;
            } else if (initial_heuristic == metrics::Algorithm::balanced_greedy_random) {
                const auto& snap = evaluation_mode_ ? g_eval_bgr_snaps.at(run - 1) : g_bgr_cache.at(active_episode_).at(run - 1);
                restore_snapshot(state, snap);
                already_started = true;
            }

            metaheuristic::boot(state, already_started);
            metrics::calculate_metrics(state);
            state.stats->save_to_file();

            sum_total_cost += state.stats->outputs.total_penalty_cost;
            max_cost_val = state.stats->inputs.max_penalty_cost;
            sum_exec_time += state.stats->outputs.execution_time;
        }

        return {
            sum_total_cost / num_runs,
            max_cost_val,
            sum_exec_time / num_runs
        };
    }

    /**
     * @brief Returns the active 1-based episode identifier.
     * @return int Active episode number.
     */
    int get_current_episode() const {
        return active_episode_;
    }
};

PYBIND11_MODULE(mmkp_sim, m) {
    py::class_<PyMMKPRunner>(m, "PyMMKPRunner")
        .def(py::init<>())
        .def("prepare_episode", &PyMMKPRunner::prepare_episode,
             py::arg("episode_id"),
             py::arg("devices_min") = 15000,
             py::arg("devices_max") = 25000,
             py::arg("services") = 100,
             py::arg("ec") = 499,
             py::arg("cc") = 1,
             py::arg("fixed_penalty") = false,
             py::arg("num_runs") = 30)
        .def("prepare_evaluation_scenario", &PyMMKPRunner::prepare_evaluation_scenario,
             py::arg("devices_count") = 20000,
             py::arg("services") = 100,
             py::arg("ec") = 499,
             py::arg("cc") = 1,
             py::arg("fixed_penalty") = false,
             py::arg("num_runs") = 30)
        .def("reset_evaluation", &PyMMKPRunner::reset_evaluation,
             py::arg("option") = 5)
        .def("reset_instance", &PyMMKPRunner::reset_instance,
             py::arg("devices_min") = 15000,
             py::arg("devices_max") = 25000,
             py::arg("services") = 100,
             py::arg("ec") = 499,
             py::arg("cc") = 1,
             py::arg("fixed_penalty") = false,
             py::arg("requested_episode") = 1,
             py::arg("option") = 5,
             py::arg("num_runs") = 30)
        .def("step_tsa", &PyMMKPRunner::step_tsa,
             py::arg("option"),
             py::arg("temperature"),
             py::arg("alpha"),
             py::arg("num_runs") = 30,
             py::arg("model_type") = "NONE")
        .def("get_current_episode", &PyMMKPRunner::get_current_episode);
}