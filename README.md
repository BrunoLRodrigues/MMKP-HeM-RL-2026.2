# MMKP Resource Allocation in IoT-EC-CC Networks

[![Documentation HTML](https://img.shields.io/badge/docs-HTML%20(Doxygen)-blue?logo=read-the-docs)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/)
[![Documentation PDF](https://img.shields.io/badge/docs-PDF%20(LaTeX)-red?logo=adobe-acrobat-reader)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/MMKP_Documentation.pdf)

🌐 **Language / Idioma:** **English** | [Português (Brasil)](README.pt-BR.md)

A C++17 simulator for solving the **Multiple Multidimensional Knapsack Problem (MMKP)** applied to resource allocation across **Internet of Things (IoT)**, **Edge Computing (EC)**, and **Cloud Computing (CC)** infrastructures. The simulator evaluates Exact (ILP via IBM ILOG CPLEX), Constructive Heuristic, and Meta-heuristic (Simulated Annealing) paradigms.

---

## 📁 Project Structure

~~~text
MMKP_IoT_EC_CC/
├── .github/workflows/
│   └── docs.yml            # GitHub Actions workflow for Doxygen (HTML + PDF)
├── data/                   # Input datasets (auto-generated if absent)
│   ├── devices/            # IoT device demands (Devices_<N>.txt)
│   ├── servers/            # EC and CC server capacities (servers_<EC>_<CC>.txt)
│   └── Services.txt        # Service templates (COST, CPU, MEM, STO, TSK)
├── docs/
│   ├── html/               # Generated Doxygen HTML documentation (git-ignored)
│   ├── latex/              # Generated Doxygen LaTeX/PDF documentation (git-ignored)
│   └── Doxyfile            # Doxygen documentation configuration
├── include/                # Header-only C++17 implementation
│   ├── exact.hpp           # CPLEX ILP mathematical formulation
│   ├── heuristic.hpp       # Constructive heuristics (GR, GRF, GCB)
│   ├── metaheuristic.hpp   # Simulated Annealing and neighborhood operators
│   ├── mmkp.hpp            # Core data structures, I/O, and metrics
│   └── tools.hpp           # CSV handler, RNG, formatting, and sorting utilities
├── results/                # Simulation outputs (CSV tables and ASCII summaries)
│   ├── EXACT/
│   ├── HEURISTIC/
│   └── METAHEURISTIC/
├── src/
│   └── main.cpp            # Benchmark entry point and simulation parameters
├── build.sh                # Automated build and execution script
├── CMakeLists.txt          # CMake build configuration
├── README.md               # Project documentation (English)
└── README.pt-BR.md         # Project documentation (Portuguese)
~~~

## ⚙️ Prerequisites

* **OS:** Linux or Windows Subsystem for Linux (WSL2 - Ubuntu 24.04 recommended)
* **Compiler:** `g++` with C++17 support
* **Build System:** Bash (`build.sh`) or `CMake` (>= 3.10)
* **Solver:** IBM ILOG CPLEX Optimization Studio 22.2 installed at:
  `/opt/ibm/ILOG/CPLEX_Studio222`

> **Note:** If your CPLEX installation is in a different directory, update the `CPLEX_INCLUDE`, `CONCERT_INCLUDE`, `CPLEX_LIB`, and `CONCERT_LIB` paths in `build.sh` and `CMakeLists.txt`.

## 🚀 How to Compile and Run

### Option 1: Using `build.sh` (Recommended)

Grant execution permission and run the script from the project root:

~~~bash
chmod +x build.sh
./build.sh
~~~

The script supports optional flags before compiling and running:
* `./build.sh -b` or `--build`: Cleans the `build/` directory.
* `./build.sh -d` or `--data`: Deletes existing datasets in `data/` (forces regeneration).
* `./build.sh -r` or `--results`: Clears previous outputs in `results/`.
* `./build.sh -a` or `--all`: Cleans `build/`, `data/`, and `results/`.
* `./build.sh -doc` or `--docs`: Regenerates Doxygen HTML and LaTeX PDF documentation.

### Option 2: Using CMake

~~~bash
mkdir -p build && cd build
cmake ..
make
cd ..
./build/MMKP_IoT-EC-CC
~~~
*(Always run the binary from the project root directory so relative paths to `data/` and `results/` resolve properly).*

## 🧠 Implemented Algorithms

| Option (`main.cpp`) | Paradigm | Algorithm | Description |
| :---: | :--- | :--- | :--- |
| `0` | `EXACT` | `SOLVER_MAXIMUM_COST` | Exact ILP solved with CPLEX maximizing avoided SLA penalties. |
| `-1` | `HEURISTIC` | `GREEDY_CB` | Deterministic greedy allocation sorted by surrogate cost-benefit utility. |
| `-2` | `HEURISTIC` | `GREEDY_RANDOM` | Stochastic baseline assigning each device to a random server. |
| `-3` | `HEURISTIC` | `GREEDY_RANDOM_FALLBACK` | Two-stage stochastic allocation prioritizing Edge (EC) with Cloud (CC) fallback. |
| `1` | `METAHEURISTIC` | `SA` + `GREEDY_CB` | Simulated Annealing initialized with `GREEDY_CB`. |
| `2` | `METAHEURISTIC` | `SA` + `GREEDY_RANDOM` | Simulated Annealing initialized with `GREEDY_RANDOM`. |
| `3` | `METAHEURISTIC` | `SA` + `GREEDY_RANDOM_FALLBACK` | Simulated Annealing initialized with `GREEDY_RANDOM_FALLBACK`. |

*Note: When executing options `1`, `2`, and `3`, the initial heuristic solution is automatically evaluated and saved under `results/HEURISTIC/` prior to running Simulated Annealing.*

## 🔧 Simulation Parameters

Simulation constants can be customized directly in `src/main.cpp`:

* `SERVICES = 100`: Number of service profiles.
* `DEVICES_START = 10000`, `DEVICES_STEP = 1000`, `DEVICES_END = 20000`: Device scaling range.
* `EC_LENGTH = 499`, `CC_LENGTH = 1`: Number of Edge and Cloud servers.
* `MAX_RUNS = 50`: Number of independent runs per instance.
* `CPLEX_TIME_LIMIT = 3600`: Maximum solver runtime in seconds (1 hour).
* `TEMPERATURE = 50.0`, `ALPHA = 0.9999`: Simulated Annealing cooling schedule.
* `FIXED_PENALTY = false`: Controls whether device SLA penalty costs are constant (`1.0`) or task-proportional.

## 📊 Simulation Results (`results/`)

In the output files stored inside the `results/` directory (`EXACT/`, `HEURISTIC/`, and `METAHEURISTIC/`):
* **First batch of execution rows (`10,000` to `20,000` devices):** Executed using a **constant penalty cost (`1.0`)** for all devices (`FIXED_PENALTY = true`).
* **Subsequent execution rows (`10,000` to `20,000` devices):** Executed using a **variable penalty cost** (`FIXED_PENALTY = false`), where each device's penalty cost is determined by the random draw of tasks generated for its assigned service profile in `data/Services.txt`.

## 📚 Documentation (Doxygen)

To generate or update the HTML (`docs/html/index.html`) and PDF (`docs/latex/refman.pdf`) documentation locally, run from the project root:

~~~bash
./build.sh --docs
~~~