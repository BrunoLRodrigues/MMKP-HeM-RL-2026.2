# MMKP Resource Allocation in IoT-EC-CC Networks

[![Documentation HTML](https://img.shields.io/badge/docs-HTML%20(Doxygen)-blue?logo=read-the-docs)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/)
[![Documentation PDF](https://img.shields.io/badge/docs-PDF%20(LaTeX)-red?logo=adobe-acrobat-reader)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/MMKP_Documentation.pdf)

🌐 **Language / Idioma:** **English** | [Português (Brasil)](README.pt-BR.md)

A C++17 high-performance simulator and Python Reinforcement Learning framework for solving the **Multiple Multidimensional Knapsack Problem (MMKP)** applied to computational resource allocation across **Internet of Things (IoT)**, **Edge Computing (EC)**, and **Cloud Computing (CC)** infrastructures.

The project evaluates **Exact** (Integer Linear Programming via IBM ILOG CPLEX), **Constructive Heuristic**, and **Meta-heuristic** (Simulated Annealing and Time-Constrained Simulated Annealing) optimization paradigms. In addition, it integrates a **Soft Actor-Critic (SAC)** Reinforcement Learning pipeline built with `Gymnasium`, `Stable-Baselines3`, and `Pybind11` to dynamically tune Simulated Annealing hyperparameters — initial temperature ($T_0$) and geometric cooling rate ($\alpha$) — based on scale-invariant statistical features of each network instance.

---

## 🎯 Problem Overview & Objectives

In IoT-EC-CC networks, $N$ heterogeneous IoT devices request computational services characterized by three multidimensional resource demands (**CPU**, **Memory**, and **Storage**) and an SLA non-service penalty cost ($c_d$). These requests must be allocated across a set of resource-constrained Edge Computing (EC) nodes and high-capacity Cloud Computing (CC) nodes such that:

1. Each IoT device is assigned to **at most one** server (single-server assignment).
2. The cumulative **CPU**, **MEM**, and **STO** demands allocated to any server never exceed its physical capacities.
3. The **total SLA penalty cost** of unserved devices is minimized (equivalently, maximizing the total avoided penalty cost of served devices).

To bridge the gap between optimal exact solutions (which are computationally expensive at scale) and fast real-time heuristics, this simulator provides:
- **In-memory C++/Python integration (`mmkp_sim`):** Eliminates process-spawning and disk I/O overhead during RL training via `Pybind11`.
- **Snapshot caching:** Persists deterministic (`GCB`) and stochastic (`BGR`) initial allocations in binary cache files so that baseline TSA and all RL-tuned TSA agents start from identical initial solutions per run.
- **Contextual Bandit RL formulation:** Maps 16 scale-invariant instance features to continuous TSA hyperparameters under a strict 1-second execution budget.

---

## 📁 Project Structure

~~~text
MMKP_IoT_EC_CC_RL/
├── .github/workflows/
│   └── docs.yml            # GitHub Actions workflow for Doxygen (HTML + PDF)
├── data/                   # Input datasets (auto-generated if absent)
│   ├── devices/            # IoT device demands (Devices_<N>.txt)
│   ├── servers/            # EC and CC server capacities (Servers_<EC>_<CC>.txt)
│   ├── training/           # Local RL episodic instances (episode_<X>/, git-ignored)
│   └── Services.txt        # Service templates (COST, CPU, MEM, STO, TSK)
├── docs/
│   ├── html/               # Generated Doxygen HTML documentation (git-ignored)
│   ├── latex/              # Generated Doxygen LaTeX/PDF documentation (git-ignored)
│   └── Doxyfile            # Doxygen documentation configuration
├── include/                # Header-only C++17 implementation
│   ├── exact.hpp           # CPLEX ILP mathematical formulation & solver config
│   ├── heuristic.hpp       # Constructive heuristics (GCB, GR, GRF, BGR)
│   ├── metaheuristic.hpp   # SA, TSA, and Shift/Insert/Swap neighborhood operators
│   ├── mmkp.hpp            # Domain entities, dataset generators, RL features, metrics
│   └── tools.hpp           # CSV handler, Mersenne Twister RNG, formatting & sorting
├── results/                # Simulation outputs (CSV tables, snapshots, ASCII summaries)
│   ├── EXACT/              # CPLEX exact solver results
│   ├── HEURISTIC/          # Constructive heuristic metrics and binary snapshot cache
│   ├── METAHEURISTIC/      # SA/TSA benchmark and RL evaluation checkpoint outputs
│   └── training/           # Local incremental RL training logs per episode (git-ignored)
├── rl/                     # Reinforcement Learning (Python) module
│   ├── models/             # Pre-trained SAC weights, replay buffers, and VecNormalize stats
│   ├── env.py              # Gymnasium Contextual Bandit environment (pybind11 wrapper)
│   ├── train.py            # Incremental SAC training script for the 6 RL agents
│   ├── evaluate.py         # Comparative evaluation script (2 Baselines vs. 6 SAC Models)
│   └── mmkp_sim*.so        # Compiled Pybind11 shared extension module (git-ignored)
├── src/
│   ├── bindings.cpp        # Pybind11 memory-persistent runner (PyMMKPRunner)
│   └── main.cpp            # Standalone C++ benchmark entry point & CLI parser
├── .gitignore              # Git ignore rules for builds, docs, Python, and RL training data
├── build.sh                # Automated build, cleanup, docs, and execution script
├── CMakeLists.txt          # CMake configuration for C++ binary and Pybind11 module
├── README.md               # Project documentation (English)
└── README.pt-BR.md         # Project documentation (Portuguese)
~~~

---

## ⚙️ Prerequisites

- **OS:** Linux or Windows Subsystem for Linux (WSL2 - Ubuntu 24.04 recommended)
- **Compiler:** `g++` with C++17 support
- **Build System:** Bash (`build.sh`) or `CMake` (>= 3.10)
- **Solver:** IBM ILOG CPLEX Optimization Studio 22.2 installed at:
  `/opt/ibm/ILOG/CPLEX_Studio222`
- **Python Environment (for RL & Pybind11 compilation):** Python 3.10+ and `python3-dev` (required for `python3-config`)
  ~~~bash
  sudo apt-get install -y python3-venv python3-dev
  python3 -m venv .venv
  source .venv/bin/activate
  pip install gymnasium stable-baselines3 pybind11 numpy
  ~~~
- **Documentation Tools (Optional, for local Doxygen builds):**
  ~~~bash
  sudo apt-get install -y doxygen graphviz texlive-latex-base texlive-fonts-recommended texlive-fonts-extra texlive-latex-extra
  ~~~

> **Note:** If your CPLEX installation is in a different directory, update the `CPLEX_INCLUDE`, `CONCERT_INCLUDE`, `CPLEX_LIB`, and `CONCERT_LIB` paths in `build.sh` (and the corresponding `*_DIR` variables in `CMakeLists.txt`).

---

## 🚀 How to Compile and Run

> **Important:** Make sure your Python virtual environment (containing `pybind11`) is **activated** before compiling, as both `build.sh` and `CMakeLists.txt` build the C++ executable (`build/MMKP_IoT-EC-CC`) and the Python extension module (`rl/mmkp_sim*.so`).

### Option 1: Using `build.sh` (Recommended)

Grant execution permission, activate the virtual environment, and run from the project root:

~~~bash
chmod +x build.sh
source .venv/bin/activate
./build.sh
~~~

The script supports optional flags before compiling:
- `./build.sh -b` or `--build`: Cleans `build/`, compiles both targets (`MMKP_IoT-EC-CC` and `mmkp_sim`), and exits without running the benchmark.
- `./build.sh -d` or `--data`: Clears benchmark datasets in `data/` (**safely preserving local `data/training/`**).
- `./build.sh -r` or `--results`: Clears benchmark outputs in `results/` (**safely preserving local `results/training/`**).
- `./build.sh -a` or `--all`: Cleans `build/`, `data/` (preserving `training/`), `results/` (preserving `training/`), and compiles without running.
- `./build.sh -doc` or `--docs`: Regenerates Doxygen HTML and LaTeX PDF documentation (`docs/html/MMKP_Documentation.pdf`).

### Option 2: Using CMake

~~~bash
source .venv/bin/activate
mkdir -p build && cd build
cmake ..
make
cd ..
./build/MMKP_IoT-EC-CC
~~~
*(Always execute binaries and Python scripts from the project root directory so relative paths to `data/`, `results/`, and `rl/models/` resolve properly.)*

### Option 3: Running the C++ Binary with Custom CLI Flags

You can override any default parameter of `src/main.cpp` directly via command-line arguments:

~~~bash
./build/MMKP_IoT-EC-CC \
  --devices-start 15000 --devices-step 1000 --devices-end 20000 \
  --ec-length 499 --cc-length 1 \
  --option-start 5 --option-end 8 \
  --max-runs 30 --temperature 50.0 --alpha 0.999 \
  --fixed-penalty false
~~~

### Option 4: Reinforcement Learning Training and Evaluation (SAC)

~~~bash
# 1. Activate the virtual environment and compile the C++ binary + Pybind11 module
source .venv/bin/activate
./build.sh --build

# 2. Run incremental SAC training for the 6 agents (auto-generates local training episodes and resumes saved models)
python3 rl/train.py

# 3. Evaluate the 2 TSA baselines and 6 trained SAC models over fixed benchmark instances
python3 rl/evaluate.py

# Optional: Evaluate specific device counts, server topologies, or checkpoint files
python3 rl/evaluate.py --devices 15000 18000 20000 --ec 499 --cc 1 --episodes 100 --model-file sac_model.zip
~~~

---

## 🧠 Implemented Algorithms

### Optimization Paradigms (`src/main.cpp` & `src/bindings.cpp`)

| Option | Paradigm | Identifier | Algorithm | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `EXACT` | `SOLVER_MAXIMUM_COST` | Exact ILP (CPLEX) | Solves the binary ILP model to maximize avoided SLA penalties subject to single-assignment and CPU/MEM/STO constraints. |
| `-1` | `HEURISTIC` | `GCB` | `greedy_cost_benefit` | Deterministic greedy allocation ranked globally by surrogate utility $u_d = c_d / (w_{\text{cpu}} + w_{\text{mem}} + w_{\text{sto}})$, with prefix-skipping and early termination. |
| `-2` | `HEURISTIC` | `GR` | `greedy_random` | Fast stochastic baseline assigning each device to one uniformly sampled server across the entire infrastructure. |
| `-3` | `HEURISTIC` | `GRF` | `greedy_random_fallback` | Two-stage stochastic allocation trying a random Edge (EC) server first, falling back to a random Cloud (CC) server if infeasible. |
| `-4` | `HEURISTIC` | `BGR` | `balanced_greedy_random` | Balanced stochastic allocation using a 50/50 coin flip per device to select the target tier (EC or CC) before sampling a server. |
| `1` | `METAHEURISTIC` | `SA` + `GCB` | Simulated Annealing | Geometric cooling SA initialized from `GCB`. |
| `2` | `METAHEURISTIC` | `SA` + `GR` | Simulated Annealing | Geometric cooling SA initialized from `GR`. |
| `3` | `METAHEURISTIC` | `SA` + `GRF` | Simulated Annealing | Geometric cooling SA initialized from `GRF`. |
| `4` | `METAHEURISTIC` | `SA` + `BGR` | Simulated Annealing | Geometric cooling SA initialized from `BGR`. |
| `5` | `METAHEURISTIC` | `TSA` + `GCB` | Timed Simulated Annealing | Time-constrained SA ($\approx 1.0\text{ s}$ total budget) initialized from `GCB`. |
| `6` | `METAHEURISTIC` | `TSA` + `GR` | Timed Simulated Annealing | Time-constrained SA ($\approx 1.0\text{ s}$ total budget) initialized from `GR`. |
| `7` | `METAHEURISTIC` | `TSA` + `GRF` | Timed Simulated Annealing | Time-constrained SA ($\approx 1.0\text{ s}$ total budget) initialized from `GRF`. |
| `8` | `METAHEURISTIC` | `TSA` + `BGR` | Timed Simulated Annealing | Time-constrained SA ($\approx 1.0\text{ s}$ total budget) initialized from `BGR`. |

*Note: When running standalone meta-heuristics (`1`–`8`) via `main.cpp`, the initial constructive heuristic is automatically evaluated and saved to `results/HEURISTIC/` before optimization begins. Deterministic methods (`0` and `-1`) execute only once per instance during multi-run benchmarks (`run_id == 0`).*

### Exact ILP Mathematical Formulation (`include/exact.hpp`)

The exact model defines binary decision variables $x_d^i \in \{0, 1\}$, where $x_d^i = 1$ if IoT device $d \in \{1, \dots, N\}$ is allocated to server $i \in \{1, \dots, M\}$ (comprising both EC and CC tiers), and $0$ otherwise. The solver maximizes the total avoided SLA penalty cost:

$$\max \sum_{d=1}^{N} \sum_{i=1}^{M} c_d \, x_d^i$$

Subject to:
- **Single-server assignment:** $\sum_{i=1}^{M} x_d^i \le 1, \quad \forall d \in \{1, \dots, N\}$
- **Multidimensional server capacities:** $\sum_{d=1}^{N} r_d^{(k)} \, x_d^i \le R_i^{(k)}, \quad \forall i \in \{1, \dots, M\},\ k \in \{\text{CPU}, \text{MEM}, \text{STO}\}$
- **Integrality:** $x_d^i \in \{0, 1\}, \quad \forall d, i$

To run only the exact CPLEX solver from the CLI:
~~~bash
./build/MMKP_IoT-EC-CC --option-start 0 --option-end 0 --cplex-time-limit 3600
~~~

### Meta-heuristic Mechanics (`include/metaheuristic.hpp`)

- **Cooling & Termination:** Geometric cooling ($T \leftarrow T \cdot \alpha$) until $T \le 10^{-3}$, executing `LOOP = 10` Markov chain iterations per temperature level (resetting the inner counter whenever a new global best improves cost by more than $10^{-9}$).
- **Acceptance Criterion:** Metropolis rule — improving moves ($\Delta < 0$) are always accepted; worsening moves are accepted with probability $\exp(-\Delta / T)$.
- **Neighborhood Operators (`tries = 10`):**
  1. **Shift:** Relocates an already served device to a different server with sufficient residual capacity ($\Delta = 0$).
  2. **Insert:** Allocates an unserved device to a feasible server, explicitly testing the last server (Cloud tier) on the final attempt ($\Delta = -c_d$).
  3. **1-for-1 Swap:** If insertion fails for an unserved device, selects a random served device, evicts it, and allocates the unserved device if capacity allows ($\Delta = c_{\text{evicted}} - c_{\text{inserted}}$).
- **Performance Optimizations:** Incremental delta cost evaluation, $O(\log N)$ in-place move reversion (`revert_neighbor`), and lazy state synchronization for tracking the best solution.
- **TSA Time Budget:** Strictly bounded by `TIME_LIMIT = 1.0 - 1e-13` seconds minus the runtime already consumed by the initial constructive heuristic.

---

## 🤖 Reinforcement Learning Framework (SAC-TSA)

The `rl/` module frames hyperparameter tuning for **Timed Simulated Annealing (TSA)** as a single-step **Contextual Bandit** ($\gamma = 0.0$) solved using **Soft Actor-Critic (SAC)** (`Stable-Baselines3`) with `VecNormalize` observation normalization.

### 1. Scale-Invariant Observation Space (16 Features)

Computed in C++ by `metrics::calculate_rl_state()` (`include/mmkp.hpp`), the 16-dimensional state vector captures instance heterogeneity and multi-tier resource pressure independently of absolute scale:

| Group | Features | Description |
| :--- | :--- | :--- |
| **Demand Heterogeneity (CV)** | `cv_d_cpu`, `cv_d_mem`, `cv_d_sto`, `cv_d_cost` | Coefficients of variation ($\sigma / \mu$) of device CPU, MEM, STO demands and penalty costs. |
| **Edge Pressure Ratios** | `ratio_de_cpu`, `ratio_de_mem`, `ratio_de_sto`, `ratio_de_dps` | Total device demands divided by total EC capacities, plus devices per EC server. |
| **Cloud Pressure Ratios** | `ratio_dc_cpu`, `ratio_dc_mem`, `ratio_dc_sto`, `ratio_dc_dps` | Total device demands divided by total CC capacities, plus devices per CC server. |
| **System Pressure Ratios** | `ratio_ds_cpu`, `ratio_ds_mem`, `ratio_ds_sto`, `ratio_ds_dps` | Total device demands divided by total (EC + CC) capacities, plus devices per server. |

### 2. Continuous Action Space & Hyperparameter Mapping

SAC outputs normalized continuous actions $u \in [-1.0, 1.0]$ mapped piecewise-linearly around the baseline center values:
- **Initial Temperature ($T_0$):** Mapped over bounds `(min=1.0, max=100.0, mid=50.0)`.
- **Cooling Rate ($\alpha$):** Parameterized logarithmically via exponent $k \in [1.0, 5.0]$ centered at $k_{\text{mid}} = 3.0$, yielding $\alpha = 1 - 10^{-k} \in [0.9, 0.99999]$ (centered at $\alpha = 0.999$).

### 3. Beta-Quadratic Coupling Reward Function

After executing `num_runs = 30` independent TSA runs per episode step in C++, the normalized mean penalty cost $\hat{C} = \text{clip}(C_{\text{avg}} / C_{\max}, 0, 1)$ and normalized mean runtime $\hat{T} = \text{clip}(T_{\text{avg}} / T_{\max}, 0, 1)$ are combined into the scalar reward:

$$R = -\hat{C} \cdot \left[1 + \hat{C}(1 - \hat{C}) \cdot \hat{T}^2\right]$$

This formulation prioritizes minimizing SLA penalty cost while smoothly penalizing excessive execution time in intermediate cost regimes.

### 4. Trained SAC Agents & Baselines (`rl/train.py` & `rl/evaluate.py`)

Six specialized SAC agents are trained incrementally across shared episodic instances and evaluated against two fixed-parameter TSA baselines (`T = 50.0`, `alpha = 0.999`):

| Approach / Agent Directory | Mode (`Model_Type`) | Initial Heuristic (`option`) | Tuned Parameters |
| :--- | :---: | :---: | :--- |
| `Baseline_1_TSA_GCB` | `NONE` | `GCB` (`5`) | Fixed ($T_0 = 50.0,\ \alpha = 0.999$) |
| `Baseline_2_TSA_BGR` | `NONE` | `BGR` (`8`) | Fixed ($T_0 = 50.0,\ \alpha = 0.999$) |
| `exp1_T_GCB` | `T` | `GCB` (`5`) | $T_0$ dynamic ($\alpha = 0.999$ fixed) |
| `exp2_alpha_GCB` | `alpha` | `GCB` (`5`) | $\alpha$ dynamic ($T_0 = 50.0$ fixed) |
| `exp3_both_GCB` | `both` | `GCB` (`5`) | $T_0$ and $\alpha$ jointly tuned |
| `exp4_T_BGR` | `T` | `BGR` (`8`) | $T_0$ dynamic ($\alpha = 0.999$ fixed) |
| `exp5_alpha_BGR` | `alpha` | `BGR` (`8`) | $\alpha$ dynamic ($T_0 = 50.0$ fixed) |
| `exp6_both_BGR` | `both` | `BGR` (`8`) | $T_0$ and $\alpha$ jointly tuned |

Each trained agent persists three artifacts in `rl/models/<agent_tag>/` to support seamless incremental resumption (`start_episode = "auto"`): `sac_model.zip`, `replay_buffer.pkl`, and `vec_normalize.pkl`.

---

## 🔧 Simulation & Training Parameters

### C++ Benchmark Parameters (`src/main.cpp`)

| Parameter (`SimulationConfig`) | CLI Flag | Default | Description |
| :--- | :--- | :---: | :--- |
| `services` | `--services` | `100` | Number of distinct service profiles in `Services.txt`. |
| `devices_start` / `step` / `end` | `--devices-start` / `-step` / `-end` | `15000` / `1000` / `20000` | IoT device scaling range for benchmark batches. |
| `ec_length` / `cc_length` | `--ec-length` / `--cc-length` | `499` / `1` | Number of Edge (EC) and Cloud (CC) servers. |
| `max_runs` / `run_id` | `--max-runs` / `--run-id` | `50` / `0` | Independent runs per instance (`run_id > 0` runs only that ID). |
| `cplex_time_limit` | `--cplex-time-limit` | `3600` | CPLEX solver wall-clock time limit in seconds (1 hour). |
| `temperature` / `alpha` | `--temperature` / `--alpha` | `50.0` / `0.9999` | Initial temperature and cooling factor for standalone SA/TSA. |
| `fixed_penalty` | `--fixed-penalty` | `false` | `true` forces constant penalty `1.0`; `false` uses task-based cost. |
| `option_start` / `option_end` | `--option-start` / `--option-end` | `0` / `3` | Inclusive range of algorithm options to execute. |
| `training` / `init_only` | `--training` / `--init-only` | `false` / `false` | Routes I/O to `training/episode_<X>/` or prints RL state only. |

### CPLEX Exact Solver Parameters (`include/exact.hpp`)

- `Threads = 1` (single-threaded deterministic comparison)
- `ClockType = 2` (wall-clock time)
- `TimeLimit = 3600` s (configurable via `--cplex-time-limit`)
- `WorkMem = 16384` MB (16 GB working memory before swapping node files)
- `MIP::Strategy::File = 3` (compressed node files on disk)
- `MIP::Tolerances::MIPGap = 1e-3` (0.1% relative optimality gap)
- `Emphasis::MIP = MIPEmphasisBalanced`

### Synthetic Dataset Distributions (`include/mmkp.hpp`)

Datasets are semicolon-delimited (`;`) and auto-generated via `std::mt19937` whenever absent:

- **Services (`Services.txt`):** `TSK` $\sim \mathcal{U}\{1, 4\}$; for each task, demands accumulate uniformly: `CPU` $\mathrel{+}= \mathcal{U}(10^{-13}, 2.5)$, `MEM` $\mathrel{+}= \mathcal{U}(10^{-13}, 2.5)$, `STO` $\mathrel{+}= \mathcal{U}(10^{-13}, 15.0)$, and `COST` $= 1.0 + \sum \mathcal{U}(10^{-13}, 2.5)$.
- **Devices (`Devices_<N>.txt`):** Each device uniformly samples one template from `Services.txt` (with `COST` overridden to `1.0` when `fixed_penalty = true`).
- **Edge Servers (`'E'`):** `CPU` $\sim \mathcal{U}(2.5, 25.0)$, `MEM` $\sim \mathcal{U}(2.5, 125.0)$, `STO` $\sim \mathcal{U}(15.0, 1000.0)$.
- **Cloud Servers (`'C'`):** `CPU` $\sim \mathcal{U}(50000, 100000)$, `MEM` $\sim \mathcal{U}(25600, 51400)$, `STO` $\sim \mathcal{U}(250000, 500000)$.

### RL Training & Evaluation Parameters (`rl/train.py` & `rl/evaluate.py`)

| Module | Parameter | Default | Description |
| :--- | :--- | :---: | :--- |
| **Training (`train.py`)** | `start_episode` / `episodes_to_train` | `"auto"` / `100` | Incremental training window (auto-detects next local episode). |
| **Training (`train.py`)** | `num_runs_per_episode` | `30` | TSA (and BGR) statistical runs averaged per episode step. |
| **Training (`train.py`)** | `devices_range` / `ec_range` / `cc_range` | `(15000, 25000)` / `(499, 499)` / `(1, 1)` | Random instance bounds sampled per training episode. |
| **SAC (`train.py`)** | `learning_rate` / `batch_size` / `buffer_size` | `1e-3` / `32` / `50000` | Optimizer step size, minibatch size, and replay buffer capacity. |
| **SAC (`train.py`)** | `gamma` / `tau` / `ent_coef` / `learning_starts` | `0.0` / `0.005` / `"auto"` / `20` | Contextual bandit discount, soft target update, auto entropy, warm-up steps. |
| **SAC (`train.py`)** | `train_freq` / `gradient_steps` / `clip_obs` | `(1, "step")` / `1` / `10.0` | Online update frequency and `VecNormalize` observation clipping. |
| **Evaluation (`evaluate.py`)** | `--devices` / `--ec` / `--cc` | `15000..20000` (step `1000`) / `499` / `1` | Benchmark scenarios evaluated across all 8 approaches (`30` runs each). |
| **Evaluation (`evaluate.py`)** | `--episodes` / `--model-file` | `None` (auto) / `"sac_model.zip"` | Checkpoint tag for output naming (`tsa_<episodes>.txt`) and model archive. |
| **Evaluation (`evaluate.py`)** | `--devices-file` / `--servers-file` | `None` | Optional file paths to auto-extract device/server counts from filenames. |

---

## 📊 Simulation Results & Output Artifacts (`results/`)

All tabular outputs are stored as semicolon-delimited (`;`) text files with 10-decimal floating-point precision (`DECIMAL_PLACES = 10`):

- **`results/EXACT/SOLVER_MAXIMUM_COST.txt`:** Exact solver runs including `Status`, `Obj_Function`, and `Gap`.
- **`results/HEURISTIC/heuristics.txt` & `results/HEURISTIC/cache/`:** Constructive heuristic metrics and binary initial-solution snapshots (`Snapshots_D<N>_EC<EC>_CC<CC>_R<R>.bin`).
- **`results/METAHEURISTIC/metaheuristics.txt` & `tsa_<episodes>.txt`:** SA/TSA benchmark results and RL checkpoint evaluation logs containing `Initial_Heuristic`, `Temperature`, `Alpha`, and `Model_Type` (`NONE`, `T`, `alpha`, or `both`).
- **Pre-trained Models & Benchmark Artifacts:** The repository versions the exact solver benchmarks (`results/EXACT/`), constructive heuristic results and binary caches (`results/HEURISTIC/`), SA/TSA and RL checkpoint evaluation results (`results/METAHEURISTIC/tsa_*.txt`), and pre-trained SAC weights, replay buffers, and normalization stats (`rl/models/`).
- **Local RL Training Episodes (`data/training/` & `results/training/`):** Due to their large disk footprint across thousands of episodes (including per-episode `initial_snapshots.bin` caches, `heuristics.txt`, and `metaheuristics.txt`), both directories are excluded from version control via `.gitignore`. They are automatically generated locally when executing `python3 rl/train.py` and are safely preserved by `build.sh` cleanup flags (`-d`, `-r`, `-a`).
- **Historical Benchmark Batches:** In the root benchmark files (`EXACT/`, `HEURISTIC/`, `METAHEURISTIC/`), the initial batch (`10,000` to `20,000` devices) reflects constant penalties (`fixed_penalty = true`, cost `1.0`), while subsequent batches reflect variable task-proportional penalties (`fixed_penalty = false`).

---

## 📚 Documentation (Doxygen)

The project uses Doxygen (with Graphviz diagrams, MathJax, and LaTeX PDF export) configured in `docs/Doxyfile`, and automatically publishes updates to GitHub Pages via `.github/workflows/docs.yml`.

To generate or update the HTML (`docs/html/index.html`) and PDF (`docs/html/MMKP_Documentation.pdf` / `docs/latex/refman.pdf`) documentation locally, run from the project root:

~~~bash
./build.sh --docs
~~~