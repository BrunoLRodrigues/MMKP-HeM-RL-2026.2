/**
 * @file mmkp.hpp
 * @brief Core data structures, synthetic dataset generators, metric trackers, and console/file display routines.
 * @details Defines the domain entities (`Device`, `Server`, `Supply`), dataset loading/generation utilities,
 * polymorphic metric collection classes (`Metrics`, `ExactMetrics`, `HeuristicMetrics`, `MetaHeuristicMetrics`),
 * and ASCII table visualization functions for the MMKP IoT-EC-CC simulator.
 */

#pragma once

#include "tools.hpp"

// Type aliases replacing C-style macros
using ii_pair = std::pair<int, int>;               ///< @brief Pair of two integers.
using id_pair = std::pair<int, double>;            ///< @brief Pair of an integer ID and a double value (e.g., utility).
using i_vector = std::vector<int>;                 ///< @brief Vector of integers.
using s_vector = std::vector<std::string>;         ///< @brief Vector of strings.
using ii_pair_vector = std::vector<ii_pair>;       ///< @brief Vector of integer-integer pairs.
using id_pair_vector = std::vector<id_pair>;       ///< @brief Vector of integer-double pairs.
using i_matrix = std::vector<i_vector>;            ///< @brief 2D matrix of integers.
using s_matrix = std::vector<s_vector>;            ///< @brief 2D matrix of strings.
using i_set = std::set<int>;                       ///< @brief Ordered set of unique integers.

/**
 * @namespace core
 * @brief Contains the fundamental domain entities for the MMKP resource allocation problem.
 */
namespace core {

    /**
     * @struct Device
     * @brief Represents an IoT device requesting computational resources for a specific service.
     */
    struct Device {
        int id;              ///< @brief Unique 1-based identifier of the device.
        double cost;         ///< @brief SLA penalty cost incurred if the device is not served.
        double cpu;          ///< @brief Required processing capacity (CPU).
        double mem;          ///< @brief Required memory capacity (MEM).
        double sto;          ///< @brief Required storage capacity (STO).
        int tasks;           ///< @brief Number of atomic tasks composing the requested service.
        int service;         ///< @brief Identifier of the service template assigned to this device.
        bool served = false; ///< @brief Flag indicating whether the device is currently allocated to a server.
        int server_id = -1;  ///< @brief ID of the server hosting this device (`-1` if unserved).

        /**
         * @brief Default constructor initializing an empty/placeholder device.
         */
        Device() : id(0), cost(0.0), cpu(0.0), mem(0.0), sto(0.0), tasks(0), service(0) {}

        /**
         * @brief Parameterized constructor initializing a device with specific resource demands.
         * @param id_ Unique identifier of the device.
         * @param cost_ Penalty cost if unserved.
         * @param cpu_ CPU demand.
         * @param mem_ Memory demand.
         * @param sto_ Storage demand.
         * @param tasks_ Number of service tasks.
         * @param service_ Service profile ID.
         */
        Device(int id_, double cost_, double cpu_, double mem_, double sto_, int tasks_, int service_)
            : id(id_), cost(cost_), cpu(cpu_), mem(mem_), sto(sto_), tasks(tasks_), service(service_) {}
    };

    /**
     * @brief Contiguous collection of `Device` instances (1-indexed, index 0 is a placeholder).
     */
    using Devices = std::vector<Device>;   

    /**
     * @struct Supply
     * @brief Tracks the cumulative resource consumption and allocated device IDs on a single server.
     */
    struct Supply {
        double cost = 0.0; ///< @brief Total avoided penalty cost of devices currently allocated to the server.
        double cpu = 0.0;  ///< @brief Currently consumed CPU capacity on the server.
        double mem = 0.0;  ///< @brief Currently consumed memory capacity on the server.
        double sto = 0.0;  ///< @brief Currently consumed storage capacity on the server.
        //double free = 0.0;
        i_set devices;     ///< @brief Set of device IDs currently served by the server.

        /**
         * @brief Default constructor initializing zero resource usage.
         */
        Supply() : cost(0.0), cpu(0.0), mem(0.0), sto(0.0) {}

        /**
         * @brief Parameterized constructor initializing specific resource usage values.
         * @param cost_ Initial accumulated cost.
         * @param cpu_ Initial consumed CPU.
         * @param mem_ Initial consumed memory.
         * @param sto_ Initial consumed storage.
         */
        Supply(double cost_,  double cpu_, double mem_, double sto_)
            : cost(cost_), cpu(cpu_), mem(mem_), sto(sto_) {}
    };

    /**
     * @struct Server
     * @brief Represents an Edge Computing (EC) or Cloud Computing (CC) server node.
     */
    struct Server {
        int id;        ///< @brief Unique 1-based identifier of the server.
        char type;     ///< @brief Server tier type (`'E'` for Edge, `'C'` for Cloud).
        double cpu;    ///< @brief Maximum total CPU capacity of the server.
        double mem;    ///< @brief Maximum total memory capacity of the server.
        double sto;    ///< @brief Maximum total storage capacity of the server.
        Supply supply; ///< @brief Current resource utilization and set of allocated devices.

        /**
         * @brief Default constructor initializing an empty/placeholder server.
         */
        Server() : id(0), type(' '), cpu(0.0), mem(0.0), sto(0.0), supply() {}

        /**
         * @brief Parameterized constructor initializing a server with given capacities.
         * @param id_ Unique 1-based server ID.
         * @param type_ Server tier character (`'E'` or `'C'`).
         * @param cpu_ Total CPU capacity.
         * @param mem_ Total memory capacity.
         * @param sto_ Total storage capacity.
         */
        Server(int id_, char type_, double cpu_, double mem_, double sto_)
            : id(id_), type(type_), cpu(cpu_), mem(mem_), sto(sto_) {}
        
        /**
         * @brief Checks if the server has enough residual capacity to serve a given device.
         * @param device The candidate device to be checked.
         * @return `true` if residual CPU, MEM, and STO capacities are sufficient; `false` otherwise.
         */
        inline bool can_serve(const Device& device) const {
            return (
                (device.cpu <= (cpu - supply.cpu))  &&
                (device.mem <= (mem - supply.mem))  &&
                (device.sto <= (sto - supply.sto))
            );
        }

        /**
         * @brief Allocates a device to this server and updates consumed resources.
         * @param device The device to allocate (modified in-place to mark `served = true` and `server_id`).
         * @return `true` if the device was successfully added; `false` if it was already present.
         */
        inline bool add_service(Device& device) { 
            auto result = supply.devices.insert(device.id);
            if (!result.second) return false;
            
            device.served = true;
            device.server_id = id;
            
            supply.cost += device.cost;
            supply.cpu += device.cpu;
            supply.mem += device.mem;
            supply.sto += device.sto;
            
            return true;
        }

        /**
         * @brief Removes a device from this server and releases its consumed resources.
         * @param device The device to remove (modified in-place to reset `served = false` and `server_id = -1`).
         * @return `true` if the device was successfully removed; `false` if it was not allocated to this server.
         */
        inline bool rmv_service(Device& device) {
            if (supply.devices.erase(device.id) == 0) return false;
            
            device.served = false;
            device.server_id = -1;
            
            supply.cost -= device.cost;
            supply.cpu -= device.cpu;
            supply.mem -= device.mem;
            supply.sto -= device.sto;

            return true;
        }
    };

    /**
     * @brief Contiguous collection of `Server` instances (1-indexed, index 0 is a placeholder).
     */
    using Servers = std::vector<Server>;   
}

/**
 * @namespace data::generate
 * @brief Generates or updates synthetic datasets for services, IoT devices, and EC/CC servers.
 */
namespace data::generate {
/**
     * @brief Provides a singleton flag indicating whether the simulator is running in RL training mode.
     * @return Reference to the static boolean training mode flag.
     */
    inline bool& is_training_mode() {
        static bool training = false;
        return training;
    }

    /**
     * @brief Counts existing episode directories in `data/training` and returns the next available episode index.
     * @return Next 1-based episode identifier.
     */
    inline int get_next_episode_number() {
        std::filesystem::path training_dir = std::filesystem::current_path() / "data" / "training";
        
        if (!std::filesystem::exists(training_dir)) {
            return 1;
        }

        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(training_dir)) {
            if (entry.is_directory()) {
                count++;
            }
        }

        int next_ep = count + 1;
        while (std::filesystem::exists(training_dir / ("episode_" + std::to_string(next_ep)))) {
            next_ep++;
        }
        
        return next_ep;
    }

    /**
     * @brief Retrieves or updates the active training episode identifier.
     * @return Reference to the static episode number.
     */
    inline int& current_episode() {
        static int episode = get_next_episode_number();
        return episode;
    }

    /**
     * @brief Resolves the active training episode ID.
     * @details If `requested_episode > 0` and its directory exists in `data/training/episode_<id>`,
     * selects that episode. Otherwise, assigns the next available episode number via `get_next_episode_number()`.
     * 
     * @param requested_episode Target episode ID (`<= 0` forces creation with the next iterator).
     * @return The resolved 1-based episode identifier.
     */
    inline int resolve_episode(int requested_episode = 0) {
        if (requested_episode > 0) {
            current_episode() = requested_episode;
        } else {
            current_episode() = get_next_episode_number();
        }
        return current_episode();
    }

    /**
     * @brief Resolves the root data directory path based on the current execution mode.
     * @return Filesystem path pointing to `data/training/episode_<X>` in training mode, or `data` otherwise.
     */
    inline std::filesystem::path get_data_path() {
        auto base = std::filesystem::current_path() / "data";
        if (is_training_mode()) {
            return base / "training" / ("episode_" + std::to_string(current_episode()));
        }
        return base;
    }

    /**
     * @brief Generates or loads network service requirement templates.
     * @details Creates random service profiles with varying task counts, memory, storage, 
     * and processing demands, persisting them to `Services.txt` inside the resolved data path.
     * 
     * @param num_services The total number of distinct service profiles to generate.
     * @return A 2D string vector containing the generated or loaded services dataset.
     */
    inline std::vector<std::vector<std::string>> services(const int num_services = 100) {
        std::filesystem::path service_path = get_data_path() / "Services.txt";
        
        if (std::filesystem::exists(service_path)) {
            return csv::read(service_path.string(), ';').value_or(std::vector<std::vector<std::string>>{});
        }

        std::vector<std::vector<std::string>> services_data;
        services_data.push_back({"#", "COST", "CPU", "MEM", "STO", "TSK"});

        for (int i = 1; i <= num_services; i++) {
            int tsk = randomness::random_number(1, 4);
            double cpu = 0.0, mem = 0.0, sto = 0.0, cost = 1.0;

            for (int j = 0; j < tsk; j++) {
                cpu += randomness::random_number(ZERO, 2.5);
                mem += randomness::random_number(ZERO, 2.5);
                sto += randomness::random_number(ZERO, 15.0);
                cost += randomness::random_number(ZERO, 2.5);
            }
            
            services_data.push_back({
                convert::to_string(i), convert::to_string(cost, DECIMAL_PLACES),
                convert::to_string(cpu, DECIMAL_PLACES), convert::to_string(mem, DECIMAL_PLACES), convert::to_string(sto, DECIMAL_PLACES),
                convert::to_string(tsk)
            });
        }
        
        if (!csv::write(service_path.string(), services_data, ';')) {
            return {};
        }

        return services_data;
    }

    /**
     * @brief Generates or loads device request data representing IoT network demands.
     * @details Uniformly samples service requirement profiles from the services dataset 
     * and assigns them to each device instance (`data/training/episode_<X>/Devices.txt` in training mode,
     * or `data/devices/Devices_<length>.txt` in benchmark mode).
     * 
     * @param length The total number of devices to generate if the file does not exist.
     * @param num_services The number of underlying service templates to sample from.
     * @param fixed_penalty Whether the penalty (cost) of each device is fixed (`1.0`) or variable.
     * @return A 2D string vector representing the devices dataset.
     */
    inline std::vector<std::vector<std::string>> devices(const int length, const int num_services = 100, bool fixed_penalty = true) {
        std::filesystem::path data_path = get_data_path();
        std::filesystem::path device_file_path = is_training_mode() ? (data_path / "Devices.txt") : (data_path / "devices" / ("Devices_" + std::to_string(length) + ".txt"));
        
        std::vector<std::vector<std::string>> services_data = services(num_services);
        if (services_data.size() <= 1) return {};

        if (std::filesystem::exists(device_file_path)) {
            auto loaded = csv::read(device_file_path.string(), ';').value_or(std::vector<std::vector<std::string>>{});

            if (fixed_penalty) {
                for (size_t i = 1; i < loaded.size(); i++) {
                    loaded[i][1] = convert::to_string(1.0, DECIMAL_PLACES);
                }
            }
            
            return loaded;
        }

        std::vector<std::vector<std::string>> devices;
        devices.push_back({"#", "COST", "CPU", "MEM", "STO", "TSK", "SVC"});
        
        for (int i = 1; i <= length; i++) {
            const auto& source_service_row = services_data.at(randomness::random_number(1, static_cast<int>(services_data.size()) - 1));

            devices.push_back({
                convert::to_string(i),    // # 
                source_service_row.at(1), // COST 
                source_service_row.at(2), // CPU 
                source_service_row.at(3), // MEM 
                source_service_row.at(4), // STO 
                source_service_row.at(5), // TASKS 
                source_service_row.at(0)  // SERVICE 
            });
        }

        if (!csv::write(device_file_path.string(), devices, ';')) {
            return {};
        }

        if (fixed_penalty) {
            for (size_t i = 1; i < devices.size(); i++) {
                devices[i][1] = convert::to_string(1.0, DECIMAL_PLACES);
            }
        }
                
        return devices;
    }

    /**
     * @brief Generates or loads server infrastructure data for Edge and Cloud nodes.
     * @details Creates capacity profiles for `ec_length` Edge servers (`'E'`) and `cc_length`
     * Cloud servers (`'C'`), saving to `data/training/episode_<X>/Servers.txt` in training mode
     * or `data/servers/Servers_<ec>_<cc>.txt` in benchmark mode.
     * 
     * @param ec_length The number of Edge Computing servers to generate.
     * @param cc_length The number of Cloud Computing servers to generate.
     * @return A 2D string vector representing the servers dataset.
     */
    inline std::vector<std::vector<std::string>> servers(const int ec_length = 499, const int cc_length = 1) {
        std::filesystem::path data_path = get_data_path();
        std::filesystem::path server_file_path = is_training_mode() ? (data_path / "Servers.txt") : (data_path / "servers" / ("Servers_" + std::to_string(ec_length) + "_" + std::to_string(cc_length) + ".txt"));

        if (std::filesystem::exists(server_file_path)) {
            return csv::read(server_file_path.string(), ';').value_or(std::vector<std::vector<std::string>>{});
        }

        std::vector<std::vector<std::string>> servers;
        servers.push_back({"#", "Type", "CPU", "MEM", "STO"});
        int size = ec_length + cc_length;
        double cpu, mem, sto;
        
        for (int it = 1; it <= size; it++) {
            if (it <= ec_length) {
                cpu = randomness::random_number(2.5, 25.0);
                mem = randomness::random_number(2.5, 125.0);
                sto = randomness::random_number(15.0, 1000.0);
            } else {
                double factor = 2.0;
                cpu = randomness::random_number(100000.0/factor, 200000.0/factor);
                mem = randomness::random_number(51200.0/factor, 102800.0/factor);
                sto = randomness::random_number(500000.0/factor, 1000000.0/factor);
            }

            servers.push_back({
                    convert::to_string(it),                  // IDX  
                    (it <= ec_length) ? "E" : "C",           // TYPE
                    convert::to_string(cpu, DECIMAL_PLACES), // CPU   
                    convert::to_string(mem, DECIMAL_PLACES), // MEM 
                    convert::to_string(sto, DECIMAL_PLACES)  // STO
                });
        }

        if (!csv::write(server_file_path.string(), servers, ';')) {
            return {};
        }
        return servers;
    }
}

/**
 * @namespace data::load
 * @brief Parses raw string datasets into strongly-typed `core::Devices` and `core::Servers` vectors.
 */
namespace data::load {

    /**
     * @brief Loads device data from disk (generating it if absent) into a vector of `core::Device` objects.
     * @param length The total number of devices to load.
     * @param num_services The number of distinct services to generate if absent. Defaults to 10.
     * @param fixed_penalty Whether the penalty (cost) of each device is fixed or can vary. Defaults to true.
     * @return A 1-indexed `core::Devices` vector containing the parsed device instances.
     */
    inline core::Devices devices(const int length, const int num_services = 100, bool fixed_penalty = true) {
        std::vector<std::vector<std::string>> load_devices = data::generate::devices(length, num_services, fixed_penalty);
        if (load_devices.size() <= 1) {
            std::cerr << "Error: Device data file not found or is empty.\n";
            return {};
        }

        core::Devices devices;
        devices.reserve(length + 1);
        devices.emplace_back(); // Placeholder for 1-based indexing.
        
        for (const auto& row : load_devices) {
            if (row.at(0) == "#") continue;
            try {
                devices.emplace_back(
                    std::stoi(row.at(0)),  // #
                    std::stod(row.at(1)),  // COST
                    std::stod(row.at(2)),  // CPU
                    std::stod(row.at(3)),  // MEM
                    std::stod(row.at(4)),  // STO
                    std::stoi(row.at(5)),  // TASKS
                    std::stoi(row.at(6))   // SERVICE
                );
            } catch (const std::exception& e) {
                std::cerr << "Error parsing device data: " << e.what() << "\n";
                break;
            }
        }

        return devices;
    }
    
    /**
     * @brief Loads Edge and Cloud server data from disk (generating it if absent).
     * @param ec The number of Edge Computing (EC) servers to load.
     * @param cc The number of Cloud Computing (CC) servers to load.
     * @return A 1-indexed `core::Servers` vector containing both EC and CC server instances.
     */
    inline core::Servers servers(const int ec = 499, const int cc = 1) {
        std::vector<std::vector<std::string>> load_servers = data::generate::servers(ec, cc);
        if (load_servers.size() <= 1) {
            std::cerr << "Error: Server data file(s) not found or are empty.\n";
            return {};
        }

        core::Servers servers;
        servers.reserve(ec + cc + 1);
        servers.emplace_back(); // Placeholder for 1-based indexing.

        for (const auto& row : load_servers) {
            if (row.at(0) == "#") continue;
            try {
                servers.emplace_back(
                    std::stoi(row.at(0)),  // #
                    row.at(1).front(),     // TYPE (extracting char safely)
                    std::stod(row.at(2)),  // CPU
                    std::stod(row.at(3)),  // MEM
                    std::stod(row.at(4))   // STO
                );
            } catch (const std::exception& e) {
                std::cerr << "Error parsing server data: " << e.what() << "\n";
                break;
            }
        }
        
        return servers;
    }    
}

/**
 * @namespace metrics
 * @brief Provides structures and routines for tracking, formatting, and persisting simulation metrics.
 */
namespace metrics {

    
    /**
     * @brief Path pointing to the root `results/` directory.
     */
    const std::filesystem::path RESULTS_PATH = std::filesystem::current_path() / "results";

    /**
     * @enum Paradigm
     * @brief Enumerates the optimization paradigms supported by the simulator.
     */
    enum class Paradigm {
        NONE,          ///< @brief Uninitialized paradigm.
        EXACT,         ///< @brief Exact mathematical programming (ILP via CPLEX).
        HEURISTIC,     ///< @brief Constructive heuristic algorithms.
        METAHEURISTIC  ///< @brief Meta-heuristic optimization algorithms.
    };

    /**
     * @brief Converts a `Paradigm` enum value to its uppercase string representation.
     * @param paradigm The `Paradigm` value to convert.
     * @return String representation of the paradigm.
     */
    inline std::string to_string(Paradigm paradigm) {
        switch (paradigm) {
            case Paradigm::EXACT:         return "EXACT";
            case Paradigm::HEURISTIC:     return "HEURISTIC";
            case Paradigm::METAHEURISTIC: return "METAHEURISTIC";
            default:                       return "NONE";
        }
    }

    /**
     * @enum Algorithm
     * @brief Identifies the specific optimization algorithm used in a simulation run.
     */
    enum class Algorithm {
        NONE,                       ///< @brief Uninitialized algorithm.
        solver_maximum_cost,        ///< @brief Exact ILP solver maximizing avoided penalty cost.
        greedy_cost_benefit,        ///< @brief Deterministic surrogate cost-benefit greedy heuristic.
        greedy_random,              ///< @brief Single-stage uniform random greedy heuristic.
        greedy_random_fallback,     ///< @brief Two-stage Edge-first random greedy heuristic with Cloud fallback.
        balanced_greedy_random,     ///< @brief Balanced 50/50 random greedy allocation between Edge and Cloud servers.
        simulated_annealing,        ///< @brief Simulated Annealing meta-heuristic.
        timed_simulated_annealing   ///< @brief Time-constrained Simulated Annealing meta-heuristic.
    };

    /**
     * @brief Converts an `Algorithm` enum value to its standardized string identifier.
     * @param algorithm The `Algorithm` value to convert.
     * @return String representation of the algorithm.
     */
    inline std::string to_string(Algorithm algorithm) {
        switch (algorithm) {
            case Algorithm::solver_maximum_cost:        return "SOLVER_MAXIMUM_COST";
            case Algorithm::greedy_random:              return "GR";
            case Algorithm::greedy_random_fallback:     return "GRF";
            case Algorithm::balanced_greedy_random:     return "BGR";
            case Algorithm::greedy_cost_benefit:        return "GCB";    
            case Algorithm::simulated_annealing:        return "SA";
            case Algorithm::timed_simulated_annealing:  return "TSA";
            default:                                    return "NONE";
        }
    }

    /**
     * @struct MetricEntry
     * @brief Represents a formatted key-value entry for displaying or exporting simulation metrics.
     */
    struct MetricEntry {
        std::string category;      ///< @brief Section category header (e.g., `"GENERAL"`, `"DEVICES"`, `"COSTS"`).
        std::string header;        ///< @brief Short column header name for CSV output.
        std::string label;         ///< @brief Human-readable row label for console/ASCII table output.
        std::string file_value;    ///< @brief Raw formatted string value for CSV persistence.
        std::string console_value; ///< @brief Decorated string value (with units/percentages) for ASCII tables.
    };

    /**
     * @struct Metrics
     * @brief Polymorphic base structure for tracking, calculating, and saving simulation metrics.
     */
    struct Metrics {
        Paradigm paradigm = Paradigm::NONE;    ///< @brief Optimization paradigm of the current run.
        Algorithm algorithm = Algorithm::NONE; ///< @brief Algorithm executed in the current run.
        int run_id = 0;                        ///< @brief Execution run identifier (1-indexed).

        /**
         * @struct Inputs
         * @brief Captures the static input instance dimensions and maximum possible penalty cost.
         */
        struct Inputs {
            int devices_count = 0;       ///< @brief Total number of IoT devices in the instance.
            int servers_ec_count = 0;    ///< @brief Total number of Edge Computing (EC) servers.
            int servers_cc_count = 0;    ///< @brief Total number of Cloud Computing (CC) servers.
            double max_penalty_cost = 0; ///< @brief Sum of penalty costs across all devices if none are served.

            Inputs& operator=(const Inputs& other) = default;
        } inputs; ///< @brief Instance input parameters.

        /**
         * @struct Outputs
         * @brief Captures the resulting performance and resource utilization metrics after allocation.
         */
        struct Outputs {
            double execution_time = 0.0;      ///< @brief Total algorithm runtime in seconds.
            int    devices_served_count = 0;  ///< @brief Total number of served devices.
            int    devices_served_ec_count = 0; ///< @brief Number of devices served by Edge (EC) servers.
            int    devices_served_cc_count = 0; ///< @brief Number of devices served by Cloud (CC) servers.
            int    servers_used_count = 0;    ///< @brief Total number of active servers hosting at least one device.
            int    servers_used_ec_count = 0; ///< @brief Number of active Edge (EC) servers.
            int    servers_used_cc_count = 0; ///< @brief Number of active Cloud (CC) servers.
            double total_penalty_cost = 0.0;  ///< @brief Total penalty cost of all unserved devices.

            Outputs& operator=(const Outputs& other) = default; 
        } outputs; ///< @brief Simulation output metrics.

        /**
         * @brief Constructs a `Metrics` object with paradigm, algorithm, and run ID.
         * @param paradigm_ Optimization paradigm.
         * @param algorithm_name_ Executed algorithm.
         * @param run_id Current run number.
         */
        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id) {}

        /**
         * @brief Constructs a `Metrics` object copying input dimensions from a base instance.
         * @param paradigm_ Optimization paradigm.
         * @param algorithm_name_ Executed algorithm.
         * @param run_id Current run number.
         * @param base Shared pointer to a base `Metrics` instance containing input parameters.
         */
        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id, const std::shared_ptr<Metrics>& base) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id) {
                if (base) inputs = base->inputs;
            }
        
        /**
         * @brief Constructs a `Metrics` object with explicit instance counts.
         * @param paradigm_ Optimization paradigm.
         * @param algorithm_name_ Executed algorithm.
         * @param run_id Current run number.
         * @param d Total device count.
         * @param s_ec Edge server count.
         * @param s_cc Cloud server count.
         */
        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id), inputs({d, s_ec, s_cc, 0}) {}
        
        /**
         * @brief Copy-like constructor initializing from an existing `Metrics` pointer and updating `run_id`.
         * @param base Shared pointer to an existing `Metrics` object.
         * @param run_id New run identifier.
         */
        Metrics(const std::shared_ptr<Metrics>& base, int run_id) 
            : paradigm(base ? base->paradigm : Paradigm::NONE), 
              algorithm(base ? base->algorithm : Algorithm::NONE), 
              run_id(run_id) {
            if (base) {
                inputs = base->inputs;
                outputs = base->outputs;
            }
        }
              
        /**
         * @brief Virtual destructor for safe polymorphic cleanup.
         */
        virtual ~Metrics() = default;
        
        /**
         * @brief Creates a polymorphic deep copy of this `Metrics` instance.
         * @return A `std::shared_ptr<Metrics>` pointing to the cloned instance.
         */
        virtual std::shared_ptr<Metrics> clone() const {
            return std::shared_ptr<Metrics>(new Metrics(*this));
        }

        /**
         * @brief Returns the output directory path corresponding to the active training episode.
         * @return Filesystem path `results/training/episode_<X>/` in training mode.
         */
        virtual std::filesystem::path get_base_directory_path() const {
            if (data::generate::is_training_mode()) {
                return RESULTS_PATH / "training" / ("episode_" + std::to_string(data::generate::current_episode()));
            }
            return RESULTS_PATH / to_string(this->paradigm);
        }

        /**
         * @brief Builds the list of formatted metric entries for console and file reporting.
         * @return Vector of `MetricEntry` records.
         */
        virtual std::vector<MetricEntry> build_metrics() const {
            int current_total = inputs.devices_count;
            int total_servers = inputs.servers_ec_count + inputs.servers_cc_count;

            std::vector<MetricEntry> m;

            m.push_back({"GENERAL", "ExeTime", "Execution Time (s)", convert::to_string(outputs.execution_time, DECIMAL_PLACES), convert::to_string(outputs.execution_time, DECIMAL_PLACES)});
            
            m.push_back({"DEVICES", "Devices", "Total", convert::to_string(current_total), std::to_string(current_total)});
            m.push_back({"DEVICES", "DServed", "Served", convert::to_percentage_string(outputs.devices_served_count, current_total), std::to_string(outputs.devices_served_count) + " (" + convert::to_percentage_string(outputs.devices_served_count, current_total) + "%)"});
            m.push_back({"DEVICES", "DServedEC", "  - on EC", convert::to_percentage_string(outputs.devices_served_ec_count, outputs.devices_served_count), std::to_string(outputs.devices_served_ec_count) + " (" + convert::to_percentage_string(outputs.devices_served_ec_count, outputs.devices_served_count) + "% of served)"});
            m.push_back({"DEVICES", "DServedCC", "  - on CC", convert::to_percentage_string(outputs.devices_served_cc_count, outputs.devices_served_count), std::to_string(outputs.devices_served_cc_count) + " (" + convert::to_percentage_string(outputs.devices_served_cc_count, outputs.devices_served_count) + "% of served)"});

            m.push_back({"SERVERS", "Servers", "Total", convert::to_string(total_servers), std::to_string(total_servers) + " (" + std::to_string(inputs.servers_ec_count) + " EC + " + std::to_string(inputs.servers_cc_count) + " CC)"});
            m.push_back({"SERVERS", "SUsed", "Used", convert::to_percentage_string(outputs.servers_used_count, total_servers), std::to_string(outputs.servers_used_count) + " (" + convert::to_percentage_string(outputs.servers_used_count, total_servers) + "%)"});
            m.push_back({"SERVERS", "SUsedEC", "  - Used EC", convert::to_percentage_string(outputs.servers_used_ec_count, inputs.servers_ec_count), std::to_string(outputs.servers_used_ec_count) + " (" + convert::to_percentage_string(outputs.servers_used_ec_count, inputs.servers_ec_count) + "% of EC)"});
            m.push_back({"SERVERS", "SUsedCC", "  - Used CC", convert::to_percentage_string(outputs.servers_used_cc_count, inputs.servers_cc_count), std::to_string(outputs.servers_used_cc_count) + " (" + convert::to_percentage_string(outputs.servers_used_cc_count, inputs.servers_cc_count) + "% of CC)"});

            m.push_back({"COSTS", "MaxCost", "MAX PENALTY COST", convert::to_string(inputs.max_penalty_cost, DECIMAL_PLACES), convert::to_string(inputs.max_penalty_cost, DECIMAL_PLACES)});
            m.push_back({"COSTS", "PenaltyCost", "PENALTY COST", convert::to_string(outputs.total_penalty_cost, DECIMAL_PLACES), convert::to_string(outputs.total_penalty_cost, DECIMAL_PLACES)});

            return m;
        }

        /**
         * @brief Returns the CSV header columns for the current metric type.
         * @return Vector of column header strings.
         */
        virtual std::vector<std::string> get_header() const {
            return {
                "Paradigm", "Algorithm", "Run", "Devices", "Servers_EC", "Servers_CC", 
                "Max_cost", "Execution_time", "Devices_served", "Devices_served_ec", 
                "Devices_served_cc", "Servers_used", "Servers_used_ec", "Servers_used_cc", "Total_cost"
            };
        }

        /**
         * @brief Collects the metric values matching the columns of `get_header()`.
         * @return Vector of formatted string values for a single CSV row.
         */
        virtual std::vector<std::string> data() const {
            return {
                to_string(paradigm),
                to_string(algorithm),
                std::to_string(run_id),
                std::to_string(inputs.devices_count),
                std::to_string(inputs.servers_ec_count),
                std::to_string(inputs.servers_cc_count),
                convert::to_string(inputs.max_penalty_cost, DECIMAL_PLACES),
                convert::to_string(outputs.execution_time, DECIMAL_PLACES),
                std::to_string(outputs.devices_served_count),
                std::to_string(outputs.devices_served_ec_count),
                std::to_string(outputs.devices_served_cc_count),
                std::to_string(outputs.servers_used_count),
                std::to_string(outputs.servers_used_ec_count),
                std::to_string(outputs.servers_used_cc_count),
                convert::to_string(outputs.total_penalty_cost, DECIMAL_PLACES)
            };
        }

        /**
         * @brief Appends the simulation metrics row to the grouped category file (`heuristics.txt` or `metaheuristics.txt`).
         */
        inline void save_to_file() const;
    };

    /**
     * @struct ExactMetrics
     * @brief Extends `Metrics` to include CPLEX solver status, objective value, and MIP optimality gap.
     */
    struct ExactMetrics : public Metrics {
        std::string status = "Unknown"; ///< @brief Termination status reported by CPLEX (e.g., `"Optimal"`).
        double objective_function = 0.0; ///< @brief Objective function value achieved by the solver.
        double gap = 1.0;                ///< @brief Relative MIP optimality gap.

        /**
         * @brief Constructs an `ExactMetrics` object copying input dimensions from a base instance.
         * @param algorithm_name_ Exact algorithm identifier.
         * @param run_id_ Execution run number.
         * @param base Base metrics pointer containing input parameters.
         */
        ExactMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base) 
            : Metrics(Paradigm::EXACT, algorithm_name_, run_id_, base) {
                if (base) inputs = base->inputs;
            }

        /**
         * @brief Constructs an `ExactMetrics` object with explicit instance dimensions.
         * @param algorithm_name_ Exact algorithm identifier.
         * @param run_id Execution run number.
         * @param d Total device count.
         * @param s_ec Edge server count.
         * @param s_cc Cloud server count.
         */
        ExactMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) : 
            Metrics(Paradigm::EXACT, algorithm_name_, run_id, d, s_ec, s_cc) {}
        
        /**
         * @brief Constructs an `ExactMetrics` object from a base pointer and run ID.
         * @param base Base metrics pointer.
         * @param run_id Execution run number.
         */
        ExactMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}
        
        /**
         * @brief Clones this `ExactMetrics` instance polymorphically.
         * @return Shared pointer to the cloned `ExactMetrics` object.
         */
        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<ExactMetrics>(new ExactMetrics(*this));
        }

        /**
         * @brief Appends CPLEX solver statistics (`Status`, `OF`, `GAP`) to the base metric entries.
         * @return Vector of `MetricEntry` records.
         */
        std::vector<MetricEntry> build_metrics() const override {
            auto m = Metrics::build_metrics();
            m.push_back({"SOLVER STATS", "Status", "Solver Status", status, status});
            m.push_back({"SOLVER STATS", "OF", "Objective Function (OF)", convert::to_string(objective_function, DECIMAL_PLACES), convert::to_string(objective_function, DECIMAL_PLACES)});
            m.push_back({"SOLVER STATS", "GAP", "MIP Gap", convert::to_string(gap, DECIMAL_PLACES), convert::to_percentage_string(gap, 1.0) + "%"});
            return m;
        }

        /**
         * @brief Appends exact solver column headers (`Status`, `Obj_Function`, `Gap`) to the CSV header.
         * @return Vector of column header strings.
         */
        std::vector<std::string> get_header() const override {
            auto h = Metrics::get_header();
            h.push_back("Status");
            h.push_back("Obj_Function");
            h.push_back("Gap");
            return h;
        }

        /**
         * @brief Appends exact solver values to the CSV row data.
         * @return Vector of formatted string values.
         */
        std::vector<std::string> data() const override {
            auto d = Metrics::data();
            d.push_back(status);
            d.push_back(convert::to_string(objective_function, DECIMAL_PLACES));
            d.push_back(convert::to_string(gap, DECIMAL_PLACES));
            return d;
        }
    };

    /**
     * @struct HeuristicMetrics
     * @brief Specialization of `Metrics` for constructive heuristic runs.
     */
    struct HeuristicMetrics : public Metrics {
        /**
         * @brief Constructs a `HeuristicMetrics` object copying input dimensions from a base instance.
         * @param algorithm_name_ Heuristic algorithm identifier.
         * @param run_id_ Execution run number.
         * @param base Base metrics pointer containing input parameters.
         */
        HeuristicMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base) :
            Metrics(Paradigm::HEURISTIC, algorithm_name_, run_id_, base) {
                if (base) inputs = base->inputs;
            }

        /**
         * @brief Constructs a `HeuristicMetrics` object with explicit instance dimensions.
         * @param algorithm_name_ Heuristic algorithm identifier.
         * @param run_id Execution run number.
         * @param d Total device count.
         * @param s_ec Edge server count.
         * @param s_cc Cloud server count.
         */
        HeuristicMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) :
            Metrics(Paradigm::HEURISTIC, algorithm_name_, run_id, d, s_ec, s_cc) {}
        
        /**
         * @brief Constructs a `HeuristicMetrics` object from a base pointer and run ID.
         * @param base Base metrics pointer.
         * @param run_id Execution run number.
         */
        HeuristicMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}

        /**
         * @brief Clones this `HeuristicMetrics` instance polymorphically.
         * @return Shared pointer to the cloned `HeuristicMetrics` object.
         */
        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<HeuristicMetrics>(new HeuristicMetrics(*this));
        }
    };

    /**
     * @struct MetaHeuristicMetrics
     * @brief Extends `Metrics` to track Simulated Annealing hyperparameters and initial heuristic selection.
     */
    struct MetaHeuristicMetrics : public Metrics {
        Algorithm heuristic = Algorithm::NONE; ///< @brief Constructive heuristic used to generate the initial solution.
        double temperature = 0.0;              ///< @brief Initial temperature \f$T_0\f$ for Simulated Annealing.
        double alpha = 0.0;                    ///< @brief Cooling rate factor \f$\alpha\f$ for Simulated Annealing.
        std::string model_type = "NONE";       ///< @brief Parameter optimized by the RL agent ("T", "\alpha", or "T\alpha").

        /**
         * @brief Constructs a `MetaHeuristicMetrics` object copying inputs from a base instance.
         * @param algorithm_name_ Meta-heuristic algorithm identifier.
         * @param run_id_ Execution run number.
         * @param base Base metrics pointer containing input parameters.
         * @param temperature_ Initial temperature \f$T_0\f$.
         * @param alpha_ Geometric cooling factor \f$\alpha\f$.
         * @param heuristic_ Initial constructive heuristic algorithm.
         * @param model_type_ Type of meta-heuristic model.
         */
        MetaHeuristicMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base, double temperature_, double alpha_, Algorithm heuristic_, const std::string& model_type_ = "NONE") 
            : Metrics(Paradigm::METAHEURISTIC, algorithm_name_, run_id_, base), temperature(temperature_), alpha(alpha_), heuristic(heuristic_), model_type(model_type_) {
                if (base) inputs = base->inputs;
            }

        /**
         * @brief Constructs a `MetaHeuristicMetrics` object with explicit instance dimensions and hyperparameters.
         * @param algorithm_name_ Meta-heuristic algorithm identifier.
         * @param run_id Execution run number.
         * @param d Total device count.
         * @param s_ec Edge server count.
         * @param s_cc Cloud server count.
         * @param temperature_ Initial temperature \f$T_0\f$.
         * @param alpha_ Geometric cooling factor \f$\alpha\f$.
         * @param heuristic_ Initial constructive heuristic algorithm.
         * @param model_type_ Type of meta-heuristic model.
         */
        MetaHeuristicMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc, double temperature_, double alpha_, Algorithm heuristic_, const std::string& model_type_ = "NONE") :
            Metrics(Paradigm::METAHEURISTIC, algorithm_name_, run_id, d, s_ec, s_cc), temperature(temperature_), alpha(alpha_), heuristic(heuristic_), model_type(model_type_) {}
        
        /**
         * @brief Constructs a `MetaHeuristicMetrics` object from a base pointer and run ID.
         * @param base Base metrics pointer.
         * @param run_id Execution run number.
         */
        MetaHeuristicMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}    
        
        /**
         * @brief Clones this `MetaHeuristicMetrics` instance polymorphically.
         * @return Shared pointer to the cloned `MetaHeuristicMetrics` object.
         */
        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<MetaHeuristicMetrics>(new MetaHeuristicMetrics(*this));
        }

        /**
         * @brief Appends meta-heuristic parameters (`Heuristic`, `Temperature`, `Alpha`) to the metric entries.
         * @return Vector of `MetricEntry` records.
         */
        std::vector<MetricEntry> build_metrics() const override {
            auto m = Metrics::build_metrics();
            std::string cat = to_string(algorithm) + " PARAMETERS";
            m.push_back({cat, "Heuristic", "Initial Solution", to_string(heuristic), to_string(heuristic)});
            m.push_back({cat, "Temperature", "Initial Temperature", convert::to_string(temperature, DECIMAL_PLACES), convert::to_string(temperature, DECIMAL_PLACES)});
            m.push_back({cat, "Alpha", "Alpha (Cooling Rate)", convert::to_string(alpha, DECIMAL_PLACES), convert::to_string(alpha, DECIMAL_PLACES)});
            return m;
        }

        /**
         * @brief Appends meta-heuristic column headers (`Initial_Heuristic`, `Temperature`, `Alpha`) to the CSV header.
         * @return Vector of column header strings.
         */
        std::vector<std::string> get_header() const override {
            auto h = Metrics::get_header();
            h.push_back("Initial_Heuristic");
            h.push_back("Temperature");
            h.push_back("Alpha");
            return h;
        }

        /**
         * @brief Appends meta-heuristic hyperparameter values to the CSV row data.
         * @return Vector of formatted string values.
         */
        std::vector<std::string> data() const override {
            auto d = Metrics::data();
            d.push_back(to_string(heuristic));
            d.push_back(convert::to_string(temperature, DECIMAL_PLACES));
            d.push_back(convert::to_string(alpha, DECIMAL_PLACES));
            return d;
        }
    };

    inline void Metrics::save_to_file() const {
        std::filesystem::path dir_path = this->get_base_directory_path();
        std::filesystem::create_directories(dir_path);

        std::string filename;
        if (paradigm == Paradigm::HEURISTIC) {
            filename = "heuristics.txt";
        } else if (paradigm == Paradigm::METAHEURISTIC) {
            filename = "metaheuristics.txt";
        } else {
            filename = to_string(this->algorithm) + ".txt";
        }

        std::filesystem::path result_path = dir_path / filename;

        std::vector<std::vector<std::string>> content;
        if (!std::filesystem::exists(result_path)) {
            auto h = this->get_header();
            if (paradigm == Paradigm::METAHEURISTIC) {
                h.push_back("Model_Type");
            }
            content.push_back(h);
        }

        auto row = this->data();
        if (paradigm == Paradigm::METAHEURISTIC) {
            if (auto meta_ptr = dynamic_cast<const MetaHeuristicMetrics*>(this)) {
                row.push_back(meta_ptr->model_type);
            } else {
                row.push_back(to_string(this->algorithm));
            }
        }
        content.push_back(row);

        file_manager::csv::append(result_path.string(), content, ';');
    }

    /**
     * @struct Result
     * @brief Encapsulates the complete system state (`Devices`, `Servers`) and polymorphic `Metrics` for a simulation run.
     */
    struct Result {
        core::Devices devices;          ///< @brief Vector of all IoT devices in the simulation.
        core::Servers servers;          ///< @brief Vector of all Edge and Cloud servers in the simulation.
        std::shared_ptr<Metrics> stats; ///< @brief Polymorphic pointer to the run's metrics tracker.
        
        /**
         * @brief Constructs a `Result` state from pre-loaded devices and servers vectors.
         * @param d Vector of devices.
         * @param s Vector of servers.
         */
        Result(core::Devices d, core::Servers s)
            : devices(std::move(d)), servers(std::move(s)) {}
            
        /**
         * @brief Deep-copy constructor cloning both entities and the polymorphic `stats` pointer.
         * @param other Source `Result` instance to copy.
         */
        Result(const Result& other)
            : devices(other.devices), servers(other.servers) {
            if (other.stats) {
                stats = other.stats->clone();
            }
        }

        /**
         * @brief Loads (or generates) an instance with the specified dimensions and initializes base metrics.
         * @param num_devices Number of IoT devices to load.
         * @param num_services Number of service templates.
         * @param num_ec Number of Edge Computing servers.
         * @param num_cc Number of Cloud Computing servers.
         * @param fixed_penalty Whether the penalty (cost) of each device is fixed or can vary.
         */ 
        Result(int num_devices, int num_services, int num_ec, int num_cc, bool fixed_penalty = true)
            : devices(data::load::devices(num_devices, num_services, fixed_penalty)), servers(data::load::servers(num_ec, num_cc)) {
            
            if (devices.size() <= 1 || servers.size() <= 1) {
                std::cerr << "Error: Data failed to load.\n";
                exit(0);
            }

            stats = std::make_shared<Metrics>(Paradigm::NONE, Algorithm::NONE, 0);
            
            int actual_ec = 0;
            int actual_cc = 0;
            for (size_t s = 1; s < this->servers.size(); ++s) {
                if (this->servers[s].type == 'E') actual_ec++;
                else if (this->servers[s].type == 'C') actual_cc++;
            }

            stats->inputs.devices_count = static_cast<int>(this->devices.size()) - 1;
            stats->inputs.servers_ec_count = actual_ec;
            stats->inputs.servers_cc_count = actual_cc;
            
            double total_cost = 0.0;
            for (size_t d = 1; d < this->devices.size(); ++d) {
                total_cost += this->devices[d].cost;
            }
            stats->inputs.max_penalty_cost = static_cast<double>(total_cost);
        }

        /**
         * @brief Deep-copy assignment operator cloning the polymorphic `stats` object.
         * @param other Source `Result` instance.
         * @return Reference to `*this`.
         */
        Result& operator=(const Result& other) {
            if (this != &other) {
                devices = other.devices;
                servers = other.servers;
                if (other.stats) stats = other.stats->clone(); 
                else stats.reset();
            }
            return *this;
        }
    };

    /**
     * @brief Computes the final simulation metrics based on the current device-to-server allocations.
     * @param state The simulation result structure holding `devices`, `servers`, and `stats`.
     */
    inline void calculate_metrics(Result& state) {
        if (!state.stats) return;

        auto& out = state.stats->outputs;

        out.devices_served_count = 0;
        out.devices_served_ec_count = 0;
        out.devices_served_cc_count = 0;
        out.servers_used_count = 0;
        out.servers_used_ec_count = 0;
        out.servers_used_cc_count = 0;
        out.total_penalty_cost = 0.0;

        for (size_t d = 1; d < state.devices.size(); ++d) {
            const auto& device = state.devices[d];
            if (device.served) {
                out.devices_served_count++;
                if (device.server_id != -1) {
                    if (state.servers[device.server_id].type == 'E') {
                        out.devices_served_ec_count++;
                    } else {
                        out.devices_served_cc_count++;
                    }
                }
            } else {
                out.total_penalty_cost += device.cost;
            }
        }

        for (size_t s = 1; s < state.servers.size(); ++s) {
            const auto& server = state.servers[s];
            if (!server.supply.devices.empty()) {
                out.servers_used_count++;
                if (server.type == 'E') {
                    out.servers_used_ec_count++;
                } else {
                    out.servers_used_cc_count++;
                }
            }
        }
    }

    /**
     * @brief Saves the final calculated metrics and ASCII summary tables to the file system.
     * @param state The simulation result structure to be saved.
     * @param show_servers_usage Flag to indicate if individual server resource usage should be printed.
     */
    inline void save_results(const Result& state, bool show_servers_usage = false);

    /**
     * @brief Prints the RL state representation of the current instance to standard output.
     * @param state The initialized simulation state containing loaded devices and servers.
     */
    inline void print_rl_state(const Result& state);

    /**
     * @brief Computes scale-invariant statistical features of the instance for the RL state vector.
     * @details Extracts normalized ratios of total resource demands to Edge, Cloud, and overall System capacities.
     * Also computes coefficients of variation (std/mean) to capture device demand and cost heterogeneity.
     * 
     * @param state The initialized simulation state containing loaded devices and servers.
     * @return A `std::vector<double>` containing the 16 dimensionless state features.
     */
    inline std::vector<double> calculate_rl_state(const Result& state) {
        const Metrics& metrics = *state.stats;
        const size_t devices_size = metrics.inputs.devices_count;
        const size_t ec_size = metrics.inputs.servers_ec_count;
        const size_t cc_size = metrics.inputs.servers_cc_count;
        const size_t ts_size = ec_size + cc_size;

        double sum_d_cpu = 0.0, sum_d_mem = 0.0, sum_d_sto = 0.0, sum_d_cost = 0.0;
        for (size_t d = 1; d < state.devices.size(); ++d) {
            sum_d_cpu  += state.devices[d].cpu;
            sum_d_mem  += state.devices[d].mem;
            sum_d_sto  += state.devices[d].sto;
            sum_d_cost += state.devices[d].cost;
        }

        double mean_d_cpu  = sum_d_cpu / devices_size;
        double mean_d_mem  = sum_d_mem / devices_size;
        double mean_d_sto  = sum_d_sto / devices_size;
        double mean_d_cost = sum_d_cost / devices_size;

        double var_d_cpu = 0.0, var_d_mem = 0.0, var_d_sto = 0.0, var_d_cost = 0.0;
        for (size_t d = 1; d < state.devices.size(); ++d) {
            var_d_cpu  += (state.devices[d].cpu - mean_d_cpu) * (state.devices[d].cpu - mean_d_cpu);
            var_d_mem  += (state.devices[d].mem - mean_d_mem) * (state.devices[d].mem - mean_d_mem);
            var_d_sto  += (state.devices[d].sto - mean_d_sto) * (state.devices[d].sto - mean_d_sto);
            var_d_cost += (state.devices[d].cost - mean_d_cost) * (state.devices[d].cost - mean_d_cost);
        }

        double cv_d_cpu  = (mean_d_cpu > ZERO)  ? std::sqrt(var_d_cpu / devices_size) / mean_d_cpu   : 0.0;
        double cv_d_mem  = (mean_d_mem > ZERO)  ? std::sqrt(var_d_mem / devices_size) / mean_d_mem   : 0.0;
        double cv_d_sto  = (mean_d_sto > ZERO)  ? std::sqrt(var_d_sto / devices_size) / mean_d_sto   : 0.0;
        double cv_d_cost = (mean_d_cost > ZERO) ? std::sqrt(var_d_cost / devices_size) / mean_d_cost : 0.0;

        double sum_ec_cpu = 0.0, sum_ec_mem = 0.0, sum_ec_sto = 0.0;
        double sum_cc_cpu = 0.0, sum_cc_mem = 0.0, sum_cc_sto = 0.0;
        for (size_t s = 1; s < state.servers.size(); ++s) {
            if (state.servers[s].type == 'E') {
                sum_ec_cpu += state.servers[s].cpu;
                sum_ec_mem += state.servers[s].mem;
                sum_ec_sto += state.servers[s].sto;
            } else {
                sum_cc_cpu += state.servers[s].cpu;
                sum_cc_mem += state.servers[s].mem;
                sum_cc_sto += state.servers[s].sto;
            }
        }
        double sum_ts_cpu = sum_ec_cpu + sum_cc_cpu, sum_ts_mem = sum_ec_mem + sum_cc_mem, sum_ts_sto = sum_ec_sto + sum_cc_sto;

        double ratio_de_cpu = (sum_ec_cpu > ZERO) ? (sum_d_cpu / sum_ec_cpu) : 0.0;
        double ratio_de_mem = (sum_ec_mem > ZERO) ? (sum_d_mem / sum_ec_mem) : 0.0;
        double ratio_de_sto = (sum_ec_sto > ZERO) ? (sum_d_sto / sum_ec_sto) : 0.0;
        double ratio_de_dps = (ec_size > 0) ? static_cast<double>(devices_size) / static_cast<double>(ec_size) : 0.0;
        
        double ratio_dc_cpu = (sum_cc_cpu > ZERO) ? (sum_d_cpu / sum_cc_cpu) : 0.0;
        double ratio_dc_mem = (sum_cc_mem > ZERO) ? (sum_d_mem / sum_cc_mem) : 0.0;
        double ratio_dc_sto = (sum_cc_sto > ZERO) ? (sum_d_sto / sum_cc_sto) : 0.0;
        double ratio_dc_dps = (cc_size > 0) ? static_cast<double>(devices_size) / static_cast<double>(cc_size) : 0.0;
        
        double ratio_ds_cpu = (sum_ts_cpu > ZERO) ? (sum_d_cpu / sum_ts_cpu) : 0.0;
        double ratio_ds_mem = (sum_ts_mem > ZERO) ? (sum_d_mem / sum_ts_mem) : 0.0;
        double ratio_ds_sto = (sum_ts_sto > ZERO) ? (sum_d_sto / sum_ts_sto) : 0.0;
        double ratio_ds_dps = (ts_size > 0) ? static_cast<double>(devices_size) / static_cast<double>(ts_size) : 0.0;

        return {
            cv_d_cpu, cv_d_mem, cv_d_sto, cv_d_cost,
            ratio_de_cpu, ratio_de_mem, ratio_de_sto, ratio_de_dps,
            ratio_dc_cpu, ratio_dc_mem, ratio_dc_sto, ratio_dc_dps,
            ratio_ds_cpu, ratio_ds_mem, ratio_ds_sto, ratio_ds_dps
        };
    }
}

/**
 * @namespace display
 * @brief Utility namespace for formatting and printing entities and ASCII metric tables to console or files.
 */
namespace display {

    /**
     * @brief Displays detailed information of a single device to standard output.
     * @param device The device instance to display.
     */
    inline void show_device(const core::Device& device) {
        std::cout << "========== Device ID: "  << device.id << " ==========\n"
                  << "  - Cost Penalty:       " << device.cost << "\n"
                  << "  - Requirements (CPU): " << device.cpu << "\n"
                  << "  - Requirements (MEM): " << device.mem << "\n"
                  << "  - Requirements (STO): " << device.sto << "\n"
                  << "  - Tasks Count:        " << device.tasks << "\n"
                  << "  - Service ID:         " << device.service << "\n"
                  << "  - State (Served):     " << (device.served ? "Yes" : "No") << "\n";

        if (device.served) {
            std::cout << "  - Assigned Server ID:   " << device.server_id << "\n";
        } else {
            std::cout << "  - Assigned Server ID:   None\n";
        }
        std::cout << "====================================\n" << std::endl;
    }

    /**
     * @brief Displays detailed information for all devices in the vector.
     * @param devices The vector of devices to display.
     */
    inline void show_devices(const core::Devices& devices) {
        std::cout << "\n--- Displaying " << (devices.size() > 0 ? devices.size() - 1 : 0) << " Devices ---\n";
        for (const auto& device : devices) {
            if (device.id == 0) continue; 
            show_device(device);
        }
    }

    /**
     * @brief Displays capacity and current demand of a single server.
     * @param server The server instance to display.
     */
    inline void show_server(const core::Server& server) {
        std::cout << "========== Server ID: " << server.id << " (Type: " << server.type << ") ==========\n"
                  << "  --- Capacity ---\n"
                  << "  - Total CPU:           " << server.cpu << "\n"
                  << "  - Total MEM:           " << server.mem << "\n"
                  << "  - Total STO:           " << server.sto << "\n\n"
                  << "  --- Current Demand ---\n"
                  << "  - Used CPU:            " << server.supply.cpu << "\n"
                  << "  - Used MEM:            " << server.supply.mem << "\n"
                  << "  - Used STO:            " << server.supply.sto << "\n"
                  << "  - Devices Served (" << server.supply.devices.size() << "): ";

        if (server.supply.devices.empty()) {
            std::cout << "None\n";
        } else {
            std::string device_list = convert::set_to_string(server.supply.devices);
            std::cout << device_list << "\n";
        }
        std::cout << "===============================================\n" << std::endl;
    }

    /**
     * @brief Displays capacity and demand information for all servers.
     * @param servers The vector of servers to display.
     */
    inline void show_servers(const core::Servers& servers) {
        std::cout << "\n--- Displaying " << (servers.size() > 0 ? servers.size() - 1 : 0) << " Servers ---\n";
        for (const auto& server : servers) {
            if (server.id == 0) continue; 
            show_server(server);
        }
    }

    /**
     * @namespace display::details
     * @brief Internal helper routines for rendering ASCII box-drawing tables.
     */
    namespace details {
        const int total_width = 72; ///< @brief Total character width of the formatted ASCII table.
        const int label_width = 26; ///< @brief Character width allocated to the left-hand label column.
       
        /**
         * @brief Prints a formatted key-value row within the ASCII table.
         * @param out The target output stream.
         * @param label The row label string.
         * @param value The row value string.
         */
        inline void print_row(std::ostream& out, const std::string& label, const std::string& value) {
            out << "| " << std::left << std::setw(label_width) << label
                << " | "<< std::left << std::setw(total_width - label_width - 7) << value
                << " |\n";
        }

        /**
         * @brief Prints the top or bottom border of the ASCII table.
         * @param out The target output stream.
         */
        inline void print_header(std::ostream& out) {
            out << "+" << std::string(total_width - 2, '=') << "+\n";
        }

        /**
         * @brief Prints a section separator line within the ASCII table.
         * @param out The target output stream.
         */
        inline void print_middle(std::ostream& out) {
            out << "+" << std::string(label_width + 2, '-') << "+"
                << std::string(total_width - label_width - 5, '-') << "+\n";
        }
        
        /**
         * @brief Prints a centered title row within the ASCII table.
         * @param out The target output stream.
         * @param title The title text to center.
         */
        inline void print_title(std::ostream& out, const std::string& title) {
            int padding_total = total_width - 3 - static_cast<int>(title.length());
            int padding_left = padding_total / 2;
            int padding_right = padding_total - padding_left + 1;
            out << "|" << std::string(padding_left, ' ') << title
                << std::string(padding_right, ' ') << "|\n";
        }
    }

    /**
     * @brief Formats and writes the complete simulation metrics table to an output stream.
     * @param metrics The metrics instance to format.
     * @param out The target output stream.
     */
    inline void show_metrics_console_or_file(const metrics::Metrics& metrics, std::ostream& out) {
        out << "\n";
        details::print_header(out);
        
        std::string title = "SIMULATION " + to_string(metrics.algorithm);
        details::print_title(out, title); 
        details::print_middle(out);

        details::print_row(out, "Paradigm", to_string(metrics.paradigm));
        details::print_row(out, "Algorithm", to_string(metrics.algorithm));
        details::print_row(out, "Simulation Run", std::to_string(metrics.run_id));

        auto entries = metrics.build_metrics();
        std::string current_category = "";

        for (const auto& entry : entries) {
            if (entry.category != current_category) {
                details::print_middle(out);
                if (entry.category != "GENERAL") {
                    details::print_title(out, entry.category);
                    details::print_middle(out);
                }
                current_category = entry.category;
            }
            details::print_row(out, entry.label, entry.console_value);
        }
        details::print_header(out);
    }

    /**
     * @brief Displays the metrics table on the console and appends it to a drawing file.
     * @param ptr Shared pointer to the simulation metrics instance.
     */
    inline void show_metrics(const std::shared_ptr<metrics::Metrics>& ptr) {
        if (!ptr) return;

        show_metrics_console_or_file(*ptr, std::cout);

        std::filesystem::path base_dir = ptr->get_base_directory_path(); 
        std::filesystem::create_directories(base_dir);
        
        std::string config_name = "D" + std::to_string(ptr->inputs.devices_count) + 
                                  "_S" + std::to_string(ptr->inputs.servers_ec_count + ptr->inputs.servers_cc_count);
        
        std::filesystem::path file_path = base_dir / (config_name + "_drawing.txt");
        
        std::ofstream file(file_path, std::ios::app); 
        if (file.is_open()) {
            show_metrics_console_or_file(*ptr, file);
        } else {
            std::cerr << "Error: Could not save drawing to " << file_path << "\n";
        }
    }

    /**
     * @brief Prints a summary table of server resource utilization percentages.
     * @param servers The vector of servers to summarize.
     */
    inline void show_servers_usage(const core::Servers& servers) {
        std::cout << "\n+------------------------------------------------------------+\n";
        std::cout << "| SERVERS RESOURCE USAGE                                     |\n";
        std::cout << "+------------------------------------------------------------+\n";
        
        for (const auto& server : servers) {
            if (server.id == 0) continue; 

            std::string cpu_pct = convert::to_percentage_string(server.supply.cpu, server.cpu, 0);
            std::string mem_pct = convert::to_percentage_string(server.supply.mem, server.mem, 0);
            std::string sto_pct = convert::to_percentage_string(server.supply.sto, server.sto, 0);

            std::cout << "  ID: " << std::left << std::setw(4) << server.id 
                      << "CPU: "  << std::right << std::setw(3) << cpu_pct << "%   "
                      << "MEM: "  << std::right << std::setw(3) << mem_pct << "%   "
                      << "STO: "  << std::right << std::setw(3) << sto_pct << "%\n";
        }
        std::cout << "+------------------------------------------------------------+\n\n";
    }

    /**
     * @brief Formats and prints the RL state feature vector to standard output.
     * @param state The initialized simulation state.
     */
    inline void show_rl_state(const metrics::Result& state) {
        const auto features = metrics::calculate_rl_state(state);
        std::cout << "RL_STATE";
        for (const double val : features) {
            std::cout << ";" << convert::to_string(val, DECIMAL_PLACES);
        }
        std::cout << "\n";
    }

    /**
     * @brief Formats and prints the episodic RL result metrics to standard output.
     * @param ptr Shared pointer to the completed simulation metrics instance.
     */
    inline void show_rl_result(const std::shared_ptr<metrics::Metrics>& ptr) {
        if (!ptr) return;
        std::cout << "RL_RESULT;"
                  << convert::to_string(ptr->outputs.total_penalty_cost, DECIMAL_PLACES) << ";"
                  << convert::to_string(ptr->inputs.max_penalty_cost, DECIMAL_PLACES) << ";"
                  << convert::to_string(ptr->outputs.execution_time, DECIMAL_PLACES) << "\n";
    }
}

inline void metrics::save_results(const Result& state, bool show_servers_usage) {
    if (!state.stats) return;
    state.stats->save_to_file();

    if (data::generate::is_training_mode()) {
        if (state.stats->paradigm == Paradigm::METAHEURISTIC) {
            display::show_rl_result(state.stats);
        }
        return;
    }

    display::show_metrics(state.stats);
    if (show_servers_usage) display::show_servers_usage(state.servers);
}

inline void metrics::print_rl_state(const Result& state) {
    display::show_rl_state(state);
}