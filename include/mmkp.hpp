#pragma once

#include "tools.hpp"

// Aliases type definitions replacing C-style macros
using ii_pair = std::pair<int, int>;
using id_pair = std::pair<int, double>;       
using i_vector = std::vector<int>;         
using s_vector = std::vector<std::string>; 
using ii_pair_vector = std::vector<ii_pair>;
using id_pair_vector = std::vector<id_pair>;
using i_matrix = std::vector<i_vector>;
using s_matrix = std::vector<s_vector>;
using i_set = std::set<int>;  

namespace core {

    struct Device {
        int id;
        double cost;
        double cpu;
        double mem;
        double sto;
        int tasks;
        int service;
        bool served = false;
        int server_id = -1;

        Device() : id(0), cost(0.0), cpu(0.0), mem(0.0), sto(0.0), tasks(0), service(0) {}
        Device(int id_, double cost_, double cpu_, double mem_, double sto_, int tasks_, int service_)
            : id(id_), cost(cost_), cpu(cpu_), mem(mem_), sto(sto_), tasks(tasks_), service(service_) {}
    };

    using Devices = std::vector<Device>;   

    struct Supply {
        double cost = 0.0;
        double cpu = 0.0;
        double mem = 0.0;
        double sto = 0.0;
        double free = 0.0;
        i_set devices;

        Supply() : cost(0.0), cpu(0.0), mem(0.0), sto(0.0) {}
        Supply(double cost_,  double cpu_, double mem_, double sto_)
            : cost(cost_), cpu(cpu_), mem(mem_), sto(sto_) {}
    };

    struct Server {
        int id;
        char type; // 'E' for Edge, 'C' for Cloud
        double cpu;
        double mem;
        double sto;
        Supply supply;

        Server() : id(0), type(' '), cpu(0.0), mem(0.0), sto(0.0), supply() {}
        Server(int id_, char type_, double cpu_, double mem_, double sto_)
            : id(id_), type(type_), cpu(cpu_), mem(mem_), sto(sto_) {}
        
        /**
         * @brief Checks if the server has enough capacity to serve a given device.
         * @param device The device to be checked.
         * @return True if the server can accommodate the device, false otherwise.
         */
        inline bool can_serve(const Device& device) const {
            return (
                (device.cpu <= (cpu - supply.cpu))  &&
                (device.mem <= (mem - supply.mem))  &&
                (device.sto <= (sto - supply.sto))
            );
        }

        /**
         * @brief Allocates a device to this server and updates resource demands.
         * @param device The device to add.
         * @return True if the device was successfully added, false if it was already served.
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

            calculate_free();
            
            return true;
        }

        /**
         * @brief Removes a device from this server and releases its resources.
         * @param device The device to remove.
         * @return True if the device was successfully removed, false if it was not found.
         */
        inline bool rmv_service(Device& device) {
            if (supply.devices.erase(device.id) == 0) return false;
            
            device.served = false;
            device.server_id = -1;
            
            supply.cost -= device.cost;
            supply.cpu -= device.cpu;
            supply.mem -= device.mem;
            supply.sto -= device.sto;

            calculate_free();
            
            return true;
        }

        inline void calculate_free() {
            supply.free = (1 - supply.cpu/cpu) + (1 - supply.mem/mem) + (1 - supply.sto/sto);
        }
    };

    using Servers = std::vector<Server>;   

    /**
     * @brief Functor to compare server pairs based on free resources (descending).
     */
    struct CompareFree {
        inline bool operator()(const std::pair<int, double>& a, const std::pair<int, double>& b) const {
            if (a.second != b.second) return a.second > b.second; 
            return a.first < b.first; 
        }
    };
    
    inline id_pair_vector sort_servers_by_free(const core::Servers& servers) {
        id_pair_vector ids;
        ids.reserve(servers.size() - 1);
        
        for (size_t i = 1; i < servers.size(); i++) {
            ids.push_back({static_cast<int>(i), servers[i].supply.free});
        }

        std::sort(ids.begin(), ids.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            if (a.second != b.second) return a.second > b.second; 
            return a.first < b.first; 
        });

        return ids;
    }
}

namespace data::generate {

    /**
     * @brief Path pointing to the main data directory.
     */
    const std::filesystem::path DATA_PATH = std::filesystem::current_path() / "data";

    /**
     * @brief Generates or loads network service requirement data.
     * @details Creates random service profiles with varying task counts, memory, storage, 
     * and processing demands. Calculates a non-service cost penalty based on these 
     * hardware requirements. The result is saved to a text file for future runs.
     * 
     * @param num_services The total number of distinct service profiles to generate.
     * @return A 2D string vector containing the generated services dataset.
     */
    inline std::vector<std::vector<std::string>> services(const int num_services = 10) {
        std::filesystem::path service_path = DATA_PATH / "Services.txt";
        if (std::filesystem::exists(service_path)) {
            return csv::read(service_path.string(), ';').value_or(std::vector<std::vector<std::string>>{});
        }

        std::vector<std::vector<std::string>> services;
        services.push_back({"#", "COST", "CPU", "MEM", "STO", "TSK"});

        for (int i = 1; i <= num_services; i++) {
            int tsk = randomness::random_number(1, 4);
            double cpu = 0.0, mem = 0.0, sto = 0.0, cos = 1.0;

            for (int j = 0; j < tsk; j++) {
                //cos += randomness::random_number(ZERO, 2.5);
                cpu += randomness::random_number(ZERO, 2.5);
                mem += randomness::random_number(ZERO, 2.5);
                sto += randomness::random_number(ZERO, 15.0);
            }
            
            services.push_back({
                convert::to_string(i), convert::to_string(cos),
                convert::to_string(cpu), convert::to_string(mem), convert::to_string(sto),
                convert::to_string(tsk)
            });
        }

        if (!csv::write(service_path.string(), services, ';')) {
            return {};
        }
        return services;
    }

    /**
     * @brief Generates or loads device request data representing mobile network users.
     * @details Merges geographic coordinate data from a base device file with randomness 
     * service requirements generated by the services routine.
     * 
     * @param length The total number of devices to generate.
     * @param num_services The number of underlying service templates to sample from.
     * @return A 2D string vector representing the comprehensive devices dataset.
     */
    inline std::vector<std::vector<std::string>> devices(const int length, const int num_services = 10) {
        std::filesystem::path device_file_path = DATA_PATH / "devices" / ("Devices_" + std::to_string(length) + ".txt");
        if (std::filesystem::exists(device_file_path)) {
            return csv::read(device_file_path.string(), ';').value_or(std::vector<std::vector<std::string>>{});
        }

        std::vector<std::vector<std::string>> data_services = services(num_services);
        if (data_services.size() <= 1) {
            return {};
        }

        std::vector<std::vector<std::string>> devices;
        devices.push_back({"#", "COST", "CPU", "MEM", "STO", "TSK", "SVC"});
        
        for (int i = 1; i <= length; i++) {
            const auto& source_service_row = data_services.at(randomness::random_number(1, static_cast<int>(data_services.size()) - 1));

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
        return devices;
    }

    /**
     * @brief Generates or loads server infrastructure data for Edge and Cloud nodes.
     * @param ec_length The number of Edge Computing servers to generate.
     * @param cc_length The number of Cloud Computing servers to generate.
     * @return A 2D string vector representing the servers dataset.
     */
    inline std::vector<std::vector<std::string>> servers(const int ec_length = 99, const int cc_length = 1) {
        std::filesystem::path server_file_path = DATA_PATH / "servers" / ("servers_" + std::to_string(ec_length) + "_" + std::to_string(cc_length) + ".txt");
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
                double factor = 4.0;
                cpu = randomness::random_number(100000.0/factor, 200000.0/factor);
                mem = randomness::random_number(51200.0/factor, 102800.0/factor);
                sto = randomness::random_number(500000.0/factor, 1000000.0/factor);
            }

            servers.push_back({
                    convert::to_string(it),        // IDX  
                    (it <= ec_length) ? "E" : "C", // TYPE
                    convert::to_string(cpu),       // CPU   
                    convert::to_string(mem),       // MEM 
                    convert::to_string(sto)        // STO
                });
        }

        if (!csv::write(server_file_path.string(), servers, ';')) {
            return {};
        }
        return servers;
    }
}

namespace data::load {

    /**
     * @brief Loads device data from generated files into a vector of Device objects.
     * @param length The total number of devices to load.
     * @param num_services The number of distinct services to generate. Defaults to 10.
     * @return A vector of Device objects containing the parsed data.
     */
    inline core::Devices devices(const int length, const int num_services = 10) {
        std::vector<std::vector<std::string>> load_devices = data::generate::devices(length, num_services);
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
     * @brief Loads edge and cloud server data from generated files.
     * @param ec The number of Edge Computing (EC) servers to load.
     * @param cc The number of Cloud Computing (CC) servers to load.
     * @return A vector of Server objects containing both EC and CC data.
     */
    inline core::Servers servers(const int ec = 99, const int cc = 1) {
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

namespace metrics {

    /**
     * @brief Path pointing to the main results directory.
     */
    const std::filesystem::path RESULTS_PATH = std::filesystem::current_path() / "results";

    /**
     * @brief Enumerates the types of simulations available.
     */
    enum class Paradigm {
        NONE,
        EXACT,          
        HEURISTIC,      
        METAHEURISTIC   
    };

    /**
     * @brief Converts a SimulationType enum value to its string representation.
     * @param type The SimulationType to convert.
     * @return String representation of the SimulationType.
     */
    inline std::string to_string(Paradigm paradigm) {
        switch (paradigm) {
            case Paradigm::EXACT:         return "Exact";
            case Paradigm::HEURISTIC:     return "Heuristic";
            case Paradigm::METAHEURISTIC: return "Metaheuristic";
            default:                       return "UnknownType";
        }
    }

    /**
     * @brief Identifies the base algorithm used for the simulation.
     */
    enum class Algorithm {
        NONE,
        solver_minimum_cost,
        greedy_cost_benefit,
        greedy_random,
        greedy_random_fallback,
        simulated_annealing
    };

    /**
     * @brief Converts the Algorithm enum to a string representation.
     */
    inline std::string to_string(Algorithm algorithm) {
        switch (algorithm) {
            case Algorithm::solver_minimum_cost:    return "EXACT";
            case Algorithm::greedy_cost_benefit:    return "GREEDY_CB";    
            case Algorithm::greedy_random:          return "GREEDY_RANDOM";
            case Algorithm::greedy_random_fallback: return "GREEDY_RANDOM_FALLBACK";
            case Algorithm::simulated_annealing:    return "SA";
            default:                                return "NONE";
        }
    }

    /**
     * @brief Represents a formatted entry for displaying or saving metrics.
     */
    struct MetricEntry {
        std::string category;
        std::string header;
        std::string label;
        std::string file_value;
        std::string console_value;
    };

    /**
     * @brief Base structure for tracking, calculating, and saving simulation metrics.
     */
    struct Metrics {
        Paradigm paradigm = Paradigm::NONE;
        Algorithm algorithm = Algorithm::NONE;
        int run_id = 0;

        struct Inputs {
            int devices_count = 0;
            int servers_ec_count = 0;
            int servers_cc_count = 0;
            int max_penalty_cost = 0;

            Inputs& operator=(const Inputs& other) = default;
        } inputs;

        struct Outputs {
            double execution_time = 0.0;
            int    devices_served_count = 0;
            int    devices_served_ec_count = 0;
            int    devices_served_cc_count = 0;
            int    servers_used_count = 0;
            int    servers_used_ec_count = 0;
            int    servers_used_cc_count = 0;
            double total_penalty_cost = 0.0; 

            Outputs& operator=(const Outputs& other) = default; 
        } outputs;

        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id) {}

        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id, const std::shared_ptr<Metrics>& base) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id) {
                if (base) inputs = base->inputs;
            }
        
        Metrics(Paradigm paradigm_, Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) 
            : paradigm(paradigm_), algorithm(algorithm_name_), run_id(run_id), inputs({d, s_ec, s_cc, 0}) {}
        
        Metrics(const std::shared_ptr<Metrics>& base, int run_id) 
            : paradigm(base ? base->paradigm : Paradigm::NONE), 
              algorithm(base ? base->algorithm : Algorithm::NONE), 
              run_id(run_id) {
            if (base) {
                inputs = base->inputs;
                outputs = base->outputs;
            }
        }
              
        virtual ~Metrics() = default;
        
        /**
         * @brief Creates a deep copy of the current Metrics object.
         * @return Shared pointer to the new Metrics object.
         */
        virtual std::shared_ptr<Metrics> clone() const {
            return std::shared_ptr<Metrics>(new Metrics(*this));
        }

        /**
         * @brief Constructs the base directory path for storing results based on the Paradigm.
         * @return std::filesystem::path containing the result base directory.
         */
        virtual std::filesystem::path get_base_directory_path() const {
            return RESULTS_PATH / to_string(this->paradigm);
        }

        /**
         * @brief Compiles a list of metric entries formatting all statistical outputs for console display.
         * @return A vector of formatted MetricEntry structures.
         */
        virtual std::vector<MetricEntry> build_metrics() const {
            int current_total = inputs.devices_count;
            int total_servers = inputs.servers_ec_count + inputs.servers_cc_count;

            std::vector<MetricEntry> m;

            m.push_back({"GENERAL", "ExeTime", "Execution Time (s)", convert::to_string(outputs.execution_time, 6), convert::to_string(outputs.execution_time, 6)});
            
            m.push_back({"DEVICES", "Devices", "Total", convert::to_string(current_total), std::to_string(current_total)});
            m.push_back({"DEVICES", "DServed", "Served", convert::to_percentage_string(outputs.devices_served_count, current_total), std::to_string(outputs.devices_served_count) + " (" + convert::to_percentage_string(outputs.devices_served_count, current_total) + "%)"});
            m.push_back({"DEVICES", "DServedEC", "  - on EC", convert::to_percentage_string(outputs.devices_served_ec_count, outputs.devices_served_count), std::to_string(outputs.devices_served_ec_count) + " (" + convert::to_percentage_string(outputs.devices_served_ec_count, outputs.devices_served_count) + "% of served)"});
            m.push_back({"DEVICES", "DServedCC", "  - on CC", convert::to_percentage_string(outputs.devices_served_cc_count, outputs.devices_served_count), std::to_string(outputs.devices_served_cc_count) + " (" + convert::to_percentage_string(outputs.devices_served_cc_count, outputs.devices_served_count) + "% of served)"});

            m.push_back({"SERVERS", "Servers", "Total", convert::to_string(total_servers), std::to_string(total_servers) + " (" + std::to_string(inputs.servers_ec_count) + " EC + " + std::to_string(inputs.servers_cc_count) + " CC)"});
            m.push_back({"SERVERS", "SUsed", "Used", convert::to_percentage_string(outputs.servers_used_count, total_servers), std::to_string(outputs.servers_used_count) + " (" + convert::to_percentage_string(outputs.servers_used_count, total_servers) + "%)"});
            m.push_back({"SERVERS", "SUsedEC", "  - Used EC", convert::to_percentage_string(outputs.servers_used_ec_count, inputs.servers_ec_count), std::to_string(outputs.servers_used_ec_count) + " (" + convert::to_percentage_string(outputs.servers_used_ec_count, inputs.servers_ec_count) + "% of EC)"});
            m.push_back({"SERVERS", "SUsedCC", "  - Used CC", convert::to_percentage_string(outputs.servers_used_cc_count, inputs.servers_cc_count), std::to_string(outputs.servers_used_cc_count) + " (" + convert::to_percentage_string(outputs.servers_used_cc_count, inputs.servers_cc_count) + "% of CC)"});

            m.push_back({"COSTS", "PenaltyCost", "PENALTY COST", convert::to_string(outputs.total_penalty_cost, 6), convert::to_string(outputs.total_penalty_cost, 6)});

            return m;
        }

        /**
         * @brief Gets the CSV header line mapped exactly to the required output standard.
         * @return A vector of header strings.
         */
        std::vector<std::string> get_header() const {
            return {
                "Paradigm", "Algorithm", "Run", "Devices", "Servers_EC", "Servers_CC", 
                "Max_cost", "Execution_time", "Devices_served", "Devices_served_ec", 
                "Devices_served_cc", "Servers_used", "Servers_used_ec", "Servers_used_cc", "Total_cost"
            };
        }

        /**
         * @brief Collects the metric values specifically ordered to match the CSV header.
         * @return A vector of data strings.
         */
        std::vector<std::string> data() const {
            return {
                to_string(paradigm),
                to_string(algorithm),
                std::to_string(run_id),
                std::to_string(inputs.devices_count),
                std::to_string(inputs.servers_ec_count),
                std::to_string(inputs.servers_cc_count),
                std::to_string(inputs.max_penalty_cost),
                convert::to_string(outputs.execution_time, 6),
                std::to_string(outputs.devices_served_count),
                std::to_string(outputs.devices_served_ec_count),
                std::to_string(outputs.devices_served_cc_count),
                std::to_string(outputs.servers_used_count),
                std::to_string(outputs.servers_used_ec_count),
                std::to_string(outputs.servers_used_cc_count),
                convert::to_string(outputs.total_penalty_cost, 6)
            };
        }

        /**
         * @brief Appends the metrics data to the specific algorithm's text file inside its paradigm directory.
         */
        inline void save_to_file() const {
            
            std::filesystem::path dir_path = this->get_base_directory_path();
            std::filesystem::path result_path = dir_path / (to_string(this->algorithm) + ".txt");
            
            std::vector<std::vector<std::string>> content;
            if (!std::filesystem::exists(result_path)) {
                content.push_back(this->get_header());
            }
            content.push_back(this->data());
            file_manager::csv::append(result_path.string(), content, ';');
        }
    };

    /**
     * @brief Extends Metrics struct to include variables specific to exact mathematical models.
     */
    struct ExactMetrics : public Metrics {
        std::string status = "Unknown"; ///< @brief Status of the CPLEX solver.
        double objective_function = 0.0;///< @brief Objective Function value.
        double gap = 1.0;               ///< @brief Final MIP relative gap.

        ExactMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base) 
            : Metrics(Paradigm::EXACT, algorithm_name_, run_id_, base) {
                if (base) inputs = base->inputs;
            }

        ExactMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) : 
            Metrics(Paradigm::EXACT, algorithm_name_, run_id, d, s_ec, s_cc) {}
        
        ExactMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}
        
        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<ExactMetrics>(new ExactMetrics(*this));
        }

        std::vector<MetricEntry> build_metrics() const override {
            auto m = Metrics::build_metrics();
            m.push_back({"SOLVER STATS", "Status", "Solver Status", status, status});
            m.push_back({"SOLVER STATS", "OF", "Objective Function (OF)", convert::to_string(objective_function, 6), convert::to_string(objective_function, 6)});
            m.push_back({"SOLVER STATS", "GAP", "MIP Gap", convert::to_string(gap, 6), convert::to_percentage_string(gap, 1.0) + "%"});
            return m;
        }
    };

    /**
     * @brief Identifies metrics specific to heuristic models.
     */
    struct HeuristicMetrics : public Metrics {
        HeuristicMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base) :
            Metrics(Paradigm::HEURISTIC, algorithm_name_, run_id_, base) {
                if (base) inputs = base->inputs;
            }

        HeuristicMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc) :
            Metrics(Paradigm::HEURISTIC, algorithm_name_, run_id, d, s_ec, s_cc) {}
        
        HeuristicMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}

        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<HeuristicMetrics>(new HeuristicMetrics(*this));
        }
    };

    /**
     * @brief Extends Metrics struct to include parameters required by meta-heuristic algorithms.
     */
    struct MetaHeuristicMetrics : public Metrics {
        Algorithm heuristic = Algorithm::NONE; ///< @brief The starting heuristic solution approach.
        double temperature = 0.0;              ///< @brief Initial simulated annealing temperature.
        double alpha = 0.0;                    ///< @brief Cooling rate.

        MetaHeuristicMetrics(Algorithm algorithm_name_, int run_id_, const std::shared_ptr<Metrics>& base, double temperature_, double alpha_, Algorithm heuristic_) 
            : Metrics(Paradigm::EXACT, algorithm_name_, run_id_, base), temperature(temperature_), alpha(alpha_), heuristic(heuristic_) {
                if (base) inputs = base->inputs;
            }

        MetaHeuristicMetrics(Algorithm algorithm_name_, int run_id, int d, int s_ec, int s_cc, double temperature_, double alpha_, Algorithm heuristic_) :
            Metrics(Paradigm::METAHEURISTIC, algorithm_name_, run_id, d, s_ec, s_cc), temperature(temperature_), alpha(alpha_), heuristic(heuristic_) {}
        
        MetaHeuristicMetrics(const std::shared_ptr<Metrics>& base, int run_id) :
            Metrics(base, run_id) {}    
        
        std::shared_ptr<Metrics> clone() const override {
            return std::shared_ptr<MetaHeuristicMetrics>(new MetaHeuristicMetrics(*this));
        }

        // CORREÇÃO: Função get_base_directory_path() removida para usar o diretório padrão da base (results/Paradigm)

        std::vector<MetricEntry> build_metrics() const override {
            auto m = Metrics::build_metrics();
            std::string cat = to_string(algorithm) + " PARAMETERS";
            m.push_back({cat, "Heuristic", "Initial Solution", to_string(heuristic), to_string(heuristic)});
            m.push_back({cat, "Temperature", "Initial Temperature", convert::to_string(temperature, 2), convert::to_string(temperature, 2)});
            m.push_back({cat, "Alpha", "Alpha (Cooling Rate)", convert::to_string(alpha, 2), convert::to_string(alpha, 2)});
            return m;
        }
    };

    /**
     * @brief Encapsulates the complete state and tracked metrics for a single simulation run.
     */
    struct Result {
        core::Devices devices;          ///< @brief System state containing all devices.
        core::Servers servers;          ///< @brief System state containing all servers.
        std::shared_ptr<Metrics> stats; ///< @brief Holds the final compiled output metrics.
        i_vector device_served;         ///< @brief List of device IDs that were successfully served.
        
        Result(core::Devices d, core::Servers s)
            : devices(std::move(d)), servers(std::move(s)) {}
            
        Result(const Result& other)
            : devices(other.devices), servers(other.servers) {
            if (other.stats) {
                stats = other.stats->clone();
            }
        }
        Result(int num_devices, int num_services, int num_ec, int num_cc)
            : devices(data::load::devices(num_devices, num_services)), servers(data::load::servers(num_ec, num_cc)) {
            
            if (devices.size() <= 1 || servers.size() <= 1) {
                std::cerr << "Error: Data failed to load.\n";
                exit(0);
            }

            stats = std::make_shared<Metrics>(Paradigm::NONE, Algorithm::NONE, 0);
            
            stats->inputs.devices_count = static_cast<int>(this->devices.size()) - 1;
            stats->inputs.servers_ec_count = num_ec;
            stats->inputs.servers_cc_count = num_cc;
            
            double total_cost = 0.0;
            for (size_t d = 1; d < this->devices.size(); ++d) {
                total_cost += this->devices[d].cost;
            }
            stats->inputs.max_penalty_cost = static_cast<int>(total_cost);
        }

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
     * @brief Computes the final simulation metrics based on the allocated system state.
     * @param state The simulation result structure holding devices, servers, and stats.
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
     * @brief Saves the final calculated metrics and generated diagrams to the file system.
     * @param state The simulation result structure to be saved.
     */
    inline void save_results(const Result& state, bool show_servers_usage = false);
}

/**
 * @brief Utility namespace for displaying struct details natively via console or files.
 */
namespace display {

    /**
     * @brief Prints device characteristics and allocation status to the console.
     * @param device The device to evaluate and display.
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
     * @brief Batch prints an array list of all devices sequentially.
     * @param devices The reference to a vector of Devices.
     */
    inline void show_devices(const core::Devices& devices) {
        std::cout << "\n--- Displaying " << (devices.size() > 0 ? devices.size() - 1 : 0) << " Devices ---\n";
        for (const auto& device : devices) {
            if (device.id == 0) continue; 
            show_device(device);
        }
    }

    /**
     * @brief Prints server capacities, demands, and served connections to the console.
     * @param server The server to evaluate and display.
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
     * @brief Batch prints an array list of all servers sequentially.
     * @param servers The reference to a vector of Servers.
     */
    inline void show_servers(const core::Servers& servers) {
        std::cout << "\n--- Displaying " << (servers.size() > 0 ? servers.size() - 1 : 0) << " Servers ---\n";
        for (const auto& server : servers) {
            if (server.id == 0) continue; 
            show_server(server);
        }
    }

    namespace details {
        const int total_width = 62;
        const int label_width = 26; 
       
        inline void print_row(std::ostream& out, const std::string& label, const std::string& value) {
            out << "| " << std::left << std::setw(label_width) << label
                << " | "<< std::left << std::setw(total_width - label_width - 7) << value
                << " |\n";
        }

        inline void print_header(std::ostream& out) {
            out << "+" << std::string(total_width - 2, '=') << "+\n";
        }

        inline void print_middle(std::ostream& out) {
            out << "+" << std::string(label_width + 2, '-') << "+"
                << std::string(total_width - label_width - 5, '-') << "+\n";
        }
        
        inline void print_title(std::ostream& out, const std::string& title) {
            int padding_total = total_width - 3 - static_cast<int>(title.length());
            int padding_left = padding_total / 2;
            int padding_right = padding_total - padding_left + 1;
            out << "|" << std::string(padding_left, ' ') << title
                << std::string(padding_right, ' ') << "|\n";
        }
    }

    /**
     * @brief Organizes structured metrics objects into human-readable visual layouts directed at an output stream.
     * @param metrics The metrics tracker entity struct.
     * @param out Standard output or filestream reference.
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
     * @brief Triggers the generation of console and text-based diagrams for generic tracking.
     * @param ptr The metrics instance pointer wrapper.
     */
    inline void show_metrics(const std::shared_ptr<metrics::Metrics>& ptr) {
        if (!ptr) return;

        show_metrics_console_or_file(*ptr, std::cout);

        std::filesystem::path base_dir = ptr->get_base_directory_path(); 
        std::filesystem::create_directories(base_dir);
        
        // Define base name using D_S configuration format
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
     * @brief Prints the resource usage percentage (CPU, MEM, STO) of each server.
     * @param servers The reference to a vector of Servers.
     */
    inline void show_servers_usage(const core::Servers& servers) {
        std::cout << "\n+------------------------------------------------------------+\n";
        std::cout << "| SERVERS RESOURCE USAGE                                     |\n";
        std::cout << "+------------------------------------------------------------+\n";
        
        for (const auto& server : servers) {
            if (server.id == 0) continue; // Ignora o índice 0

            // Utiliza o convert::to_percentage_string do tools.hpp com precisão 0
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
}

inline void metrics::save_results(const Result& state, bool show_servers_usage) {
    if (!state.stats) return;
    state.stats->save_to_file();
    display::show_metrics(state.stats);
    if (show_servers_usage) display::show_servers_usage(state.servers);
}