# Alocação de Recursos MMKP em Redes IoT-EC-CC

[![Documentation HTML](https://img.shields.io/badge/docs-HTML%20(Doxygen)-blue?logo=read-the-docs)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/)
[![Documentation PDF](https://img.shields.io/badge/docs-PDF%20(LaTeX)-red?logo=adobe-acrobat-reader)](https://brunolrodrigues.github.io/MMKP-HeM-RL-2026.2/MMKP_Documentation.pdf)

🌐 **Idioma / Language:** [English](README.md) | **Português (Brasil)**

Simulador de alto desempenho em C++17 e *framework* de Aprendizado por Reforço em Python para a resolução do **Problema das Múltiplas Mochilas Multidimensionais (MMKP)** aplicado à alocação de recursos computacionais em infraestruturas de **Internet das Coisas (IoT)**, **Computação de Borda (EC)** e **Computação em Nuvem (CC)**.

O projeto avalia paradigmas de otimização **Exatos** (Programação Linear Inteira via IBM ILOG CPLEX), **Heurísticas Construtivas** e **Meta-heurísticas** (*Simulated Annealing* e *Timed Simulated Annealing*). Além disso, integra um *pipeline* de Aprendizado por Reforço baseado em **Soft Actor-Critic (SAC)** com `Gymnasium`, `Stable-Baselines3` e `Pybind11` para calibrar dinamicamente os hiperparâmetros do *Simulated Annealing* — temperatura inicial ($T_0$) e fator de resfriamento geométrico ($\alpha$) — a partir de atributos estatísticos invariantes à escala de cada instância da rede.

---

## 🎯 Visão Geral do Problema e Objetivos

Em redes IoT-EC-CC, $N$ dispositivos IoT heterogêneos requisitam serviços caracterizados por três demandas multidimensionais de recursos (**CPU**, **Memória** e **Armazenamento**) e um custo de penalidade de SLA por não atendimento ($c_d$). Essas requisições devem ser alocadas em servidores de Computação de Borda (EC), com recursos restritos, e servidores de Computação em Nuvem (CC), de alta capacidade, obedecendo às seguintes restrições:

1. Cada dispositivo IoT pode ser alocado a **no máximo um** servidor (atribuição única).
2. O consumo acumulado de **CPU**, **MEM** e **STO** em qualquer servidor não pode exceder sua capacidade física.
3. O **custo total de penalidade de SLA** dos dispositivos não atendidos deve ser minimizado (de forma equivalente, maximizando o custo de penalidade evitado dos dispositivos atendidos).

Para conciliar a qualidade das soluções exatas (computacionalmente custosas em larga escala) com a rapidez exigida em cenários reais, o simulador oferece:
- **Integração C++/Python em memória (`mmkp_sim`):** Elimina o *overhead* de criação de subprocessos e leitura/escrita em disco durante o treinamento de RL por meio do `Pybind11`.
- **Cache de *snapshots* iniciais:** Salva em arquivos binários as alocações iniciais das heurísticas determinística (`GCB`) e estocástica (`BGR`), garantindo que as *baselines* e todos os modelos SAC iniciem exatamente das mesmas soluções iniciais em cada rodada.
- **Formulação de *Contextual Bandit*:** Mapeia 16 *features* invariantes à escala para os hiperparâmetros contínuos do TSA sob um orçamento rigoroso de 1 segundo de execução.

---

## 📁 Estrutura do Projeto

~~~text
MMKP_IoT_EC_CC_RL/
├── .github/workflows/
│   └── docs.yml            # Workflow do GitHub Actions para o Doxygen (HTML + PDF)
├── data/                   # Conjuntos de dados de entrada (gerados automaticamente)
│   ├── devices/            # Demandas de dispositivos IoT (Devices_<N>.txt)
│   ├── servers/            # Capacidades de servidores EC e CC (Servers_<EC>_<CC>.txt)
│   ├── training/           # Instâncias episódicas locais de RL (episode_<X>/, ignorada no git)
│   └── Services.txt        # Perfis de serviços (COST, CPU, MEM, STO, TSK)
├── docs/
│   ├── html/               # Documentação HTML gerada pelo Doxygen (ignorada no git)
│   ├── latex/              # Documentação LaTeX/PDF gerada pelo Doxygen (ignorada no git)
│   └── Doxyfile            # Arquivo de configuração do Doxygen
├── include/                # Implementação C++17 Header-Only
│   ├── exact.hpp           # Formulação matemática ILP e parâmetros do CPLEX
│   ├── heuristic.hpp       # Heurísticas construtivas (GCB, GR, GRF, BGR)
│   ├── metaheuristic.hpp   # SA, TSA e operadores de vizinhança (Shift/Insert/Swap)
│   ├── mmkp.hpp            # Entidades de domínio, geradores de dados, features RL e métricas
│   └── tools.hpp           # Utilitários de CSV, gerador Mersenne Twister, formatação e ordenação
├── results/                # Saídas das simulações (tabelas CSV, snapshots e resumos ASCII)
│   ├── EXACT/              # Resultados do solver exato CPLEX
│   ├── HEURISTIC/          # Métricas das heurísticas construtivas e cache binário
│   ├── METAHEURISTIC/      # Resultados de benchmark SA/TSA e checkpoints de avaliação RL
│   └── training/           # Logs locais incrementais de treino por episódio (ignorada no git)
├── rl/                     # Módulo Python de Aprendizado por Reforço
│   ├── models/             # Pesos SAC pré-treinados, replay buffers e estatísticas VecNormalize
│   ├── env.py              # Ambiente Gymnasium (wrapper do pybind11)
│   ├── train.py            # Script de treinamento incremental dos 6 agentes SAC
│   ├── evaluate.py         # Script de avaliação comparativa (2 Baselines vs. 6 Modelos SAC)
│   └── mmkp_sim*.so        # Módulo compartilhado Pybind11 compilado (ignorado no git)
├── src/
│   ├── bindings.cpp        # Wrapper persistente em memória via Pybind11 (PyMMKPRunner)
│   └── main.cpp            # Ponto de entrada C++ para benchmarks e parser de CLI
├── .gitignore              # Regras de exclusão para builds, docs, Python e dados de treino RL
├── build.sh                # Script automatizado de build, limpeza, docs e execução
├── CMakeLists.txt          # Configuração CMake para o executável C++ e módulo Pybind11
├── README.md               # Documentação do projeto (Inglês)
└── README.pt-BR.md         # Documentação do projeto (Português)
~~~

---

## ⚙️ Pré-requisitos

- **Sistema Operacional:** Linux ou Windows Subsystem for Linux (WSL2 - Ubuntu 24.04 recomendado)
- **Compilador:** `g++` com suporte a C++17
- **Sistema de Build:** Bash (`build.sh`) ou `CMake` (>= 3.10)
- **Solver:** IBM ILOG CPLEX Optimization Studio 22.2 instalado em:
  `/opt/ibm/ILOG/CPLEX_Studio222`
- **Ambiente Python (para RL e compilação do Pybind11):** Python 3.10+ e `python3-dev` (necessário para o `python3-config`)
  ~~~bash
  sudo apt-get install -y python3-venv python3-dev
  python3 -m venv .venv
  source .venv/bin/activate
  pip install gymnasium stable-baselines3 pybind11 numpy
  ~~~
- **Ferramentas de Documentação (Opcional, para geração local do Doxygen):**
  ~~~bash
  sudo apt-get install -y doxygen graphviz texlive-latex-base texlive-fonts-recommended texlive-fonts-extra texlive-latex-extra
  ~~~

> **Nota:** Caso o seu CPLEX esteja instalado em outro diretório, atualize as variáveis `CPLEX_INCLUDE`, `CONCERT_INCLUDE`, `CPLEX_LIB` e `CONCERT_LIB` em `build.sh` (e as variáveis `*_DIR` correspondentes em `CMakeLists.txt`).

---

## 🚀 Como Compilar e Executar

> **Importante:** Certifique-se de que o ambiente virtual Python (com `pybind11` instalado) esteja **ativado** antes de compilar, pois tanto o `build.sh` quanto o `CMakeLists.txt` compilam simultaneamente o executável C++ (`build/MMKP_IoT-EC-CC`) e a extensão Python (`rl/mmkp_sim*.so`).

### Opção 1: Utilizando o `build.sh` (Recomendado)

Conceda permissão de execução, ative o ambiente virtual e execute na raiz do projeto:

~~~bash
chmod +x build.sh
source .venv/bin/activate
./build.sh
~~~

O script aceita flags opcionais antes da compilação:
- `./build.sh -b` ou `--build`: Limpa `build/`, compila os dois alvos (`MMKP_IoT-EC-CC` e `mmkp_sim`) e encerra sem executar o benchmark.
- `./build.sh -d` ou `--data`: Remove os datasets de benchmark em `data/` (**preservando com segurança a pasta local `data/training/`**).
- `./build.sh -r` ou `--results`: Remove os resultados de benchmark em `results/` (**preservando com segurança a pasta local `results/training/`**).
- `./build.sh -a` ou `--all`: Limpa `build/`, `data/` (preservando `training/`), `results/` (preservando `training/`) e apenas compila o projeto.
- `./build.sh -doc` ou `--docs`: Gera novamente a documentação Doxygen em HTML e PDF LaTeX (`docs/html/MMKP_Documentation.pdf`).

### Opção 2: Utilizando CMake

~~~bash
source .venv/bin/activate
mkdir -p build && cd build
cmake ..
make
cd ..
./build/MMKP_IoT-EC-CC
~~~
*(Execute sempre os binários e scripts Python a partir do diretório raiz do projeto para que os caminhos relativos para `data/`, `results/` e `rl/models/` sejam resolvidos corretamente.)*

### Opção 3: Executando o Binário C++ com Parâmetros via Linha de Comando

Qualquer parâmetro padrão de `src/main.cpp` pode ser sobrescrito diretamente via CLI:

~~~bash
./build/MMKP_IoT-EC-CC \
  --devices-start 15000 --devices-step 1000 --devices-end 20000 \
  --ec-length 499 --cc-length 1 \
  --option-start 5 --option-end 8 \
  --max-runs 30 --temperature 50.0 --alpha 0.999 \
  --fixed-penalty false
~~~

### Opção 4: Treinamento e Avaliação por Aprendizado por Reforço (SAC)

~~~bash
# 1. Ative o ambiente virtual e compile o executável C++ + módulo Pybind11
source .venv/bin/activate
./build.sh --build

# 2. Inicie o treinamento incremental dos 6 agentes SAC (gera os episódios localmente e retoma os modelos salvos)
python3 rl/train.py

# 3. Avalie as 2 baselines TSA e os 6 modelos SAC treinados nos cenários fixos de validação
python3 rl/evaluate.py

# Opcional: Avaliar quantidades específicas de dispositivos, servidores ou checkpoints
python3 rl/evaluate.py --devices 15000 18000 20000 --ec 499 --cc 1 --episodes 100 --model-file sac_model.zip
~~~

---

## 🧠 Algoritmos Implementados

### Paradigmas de Otimização (`src/main.cpp` e `src/bindings.cpp`)

| Opção | Paradigma | Identificador | Algoritmo | Descrição |
| :---: | :--- | :---: | :--- | :--- |
| `0` | `EXACT` | `SOLVER_MAXIMUM_COST` | ILP Exato (CPLEX) | Resolve o modelo binário ILP maximizando o total de penalidades evitadas, respeitando atribuição única e capacidades de CPU/MEM/STO. |
| `-1` | `HEURISTIC` | `GCB` | `greedy_cost_benefit` | Alocação gulosa determinística ordenada globalmente pela pseudo-utilidade $u_d = c_d / (w_{\text{cpu}} + w_{\text{mem}} + w_{\text{sto}})$, com *prefix-skipping* e parada antecipada. |
| `-2` | `HEURISTIC` | `GR` | `greedy_random` | *Baseline* estocástica rápida que tenta alocar cada dispositivo em um servidor sorteado uniformemente em toda a infraestrutura. |
| `-3` | `HEURISTIC` | `GRF` | `greedy_random_fallback` | Alocação estocástica em dois estágios priorizando um servidor de Borda (EC) aleatório, com *fallback* para a Nuvem (CC) em caso de inviabilidade. |
| `-4` | `HEURISTIC` | `BGR` | `balanced_greedy_random` | Alocação estocástica balanceada que usa sorteio 50/50 por dispositivo para escolher a camada alvo (EC ou CC) antes de sortear o servidor. |
| `1` | `METAHEURISTIC` | `SA` + `GCB` | *Simulated Annealing* | SA com resfriamento geométrico inicializado via `GCB`. |
| `2` | `METAHEURISTIC` | `SA` + `GR` | *Simulated Annealing* | SA com resfriamento geométrico inicializado via `GR`. |
| `3` | `METAHEURISTIC` | `SA` + `GRF` | *Simulated Annealing* | SA com resfriamento geométrico inicializado via `GRF`. |
| `4` | `METAHEURISTIC` | `SA` + `BGR` | *Simulated Annealing* | SA com resfriamento geométrico inicializado via `BGR`. |
| `5` | `METAHEURISTIC` | `TSA` + `GCB` | *Timed Simulated Annealing* | SA limitado por tempo ($\approx 1.0\text{ s}$ no total) inicializado via `GCB`. |
| `6` | `METAHEURISTIC` | `TSA` + `GR` | *Timed Simulated Annealing* | SA limitado por tempo ($\approx 1.0\text{ s}$ no total) inicializado via `GR`. |
| `7` | `METAHEURISTIC` | `TSA` + `GRF` | *Timed Simulated Annealing* | SA limitado por tempo ($\approx 1.0\text{ s}$ no total) inicializado via `GRF`. |
| `8` | `METAHEURISTIC` | `TSA` + `BGR` | *Timed Simulated Annealing* | SA limitado por tempo ($\approx 1.0\text{ s}$ no total) inicializado via `BGR`. |

*Nota: Ao executar as meta-heurísticas (`1`–`8`) diretamente pelo `main.cpp`, a heurística construtiva inicial é avaliada e salva automaticamente em `results/HEURISTIC/` antes do início da otimização. Métodos determinísticos (`0` e `-1`) são executados apenas uma vez por instância em benchmarks com múltiplas rodadas (`run_id == 0`).*

### Formulação Matemática ILP Exata (`include/exact.hpp`)

O modelo exato define variáveis de decisão binárias $x_d^i \in \{0, 1\}$, onde $x_d^i = 1$ se o dispositivo IoT $d \in \{1, \dots, N\}$ é alocado ao servidor $i \in \{1, \dots, M\}$ (abrangendo as camadas EC e CC), e $0$ caso contrário. O solver maximiza o total de custos de penalidade de SLA evitados:

$$\max \sum_{d=1}^{N} \sum_{i=1}^{M} c_d \, x_d^i$$

Sujeito a:
- **Atribuição única por dispositivo:** $\sum_{i=1}^{M} x_d^i \le 1, \quad \forall d \in \{1, \dots, N\}$
- **Capacidades multidimensionais dos servidores:** $\sum_{d=1}^{N} r_d^{(k)} \, x_d^i \le R_i^{(k)}, \quad \forall i \in \{1, \dots, M\},\ k \in \{\text{CPU}, \text{MEM}, \text{STO}\}$
- **Domínio binário:** $x_d^i \in \{0, 1\}, \quad \forall d, i$

Para executar apenas o solver exato CPLEX via linha de comando:
~~~bash
./build/MMKP_IoT-EC-CC --option-start 0 --option-end 0 --cplex-time-limit 3600
~~~

### Mecânica das Meta-heurísticas (`include/metaheuristic.hpp`)

- **Resfriamento e Critério de Parada:** Resfriamento geométrico ($T \leftarrow T \cdot \alpha$) até $T \le 10^{-3}$, com comprimento de cadeia de Markov `LOOP = 10` iterações por patamar de temperatura (reiniciando o laço interno sempre que um novo melhor custo global melhora em mais de $10^{-9}$).
- **Critério de Aceitação:** Regra de Metropolis — movimentos de melhora ($\Delta < 0$) são sempre aceitos; movimentos de piora são aceitos com probabilidade $\exp(-\Delta / T)$.
- **Operadores de Vizinhança (`tries = 10`):**
  1. **Shift:** Move um dispositivo já atendido para outro servidor com capacidade residual disponível ($\Delta = 0$).
  2. **Insert:** Aloca um dispositivo não atendido em um servidor viável, testando explicitamente o último servidor (camada Nuvem) na última tentativa ($\Delta = -c_d$).
  3. **Swap 1-para-1:** Caso a inserção falhe para um dispositivo não atendido, seleciona aleatoriamente um dispositivo atendido, remove-o e aloca o novo dispositivo se houver capacidade ($\Delta = c_{\text{removido}} - c_{\text{inserido}}$).
- **Otimizações de Desempenho:** Avaliação incremental de custo ($\Delta$), reversão de movimento *in-place* em $O(\log N)$ (`revert_neighbor`) e sincronização preguiçosa (*lazy sync*) da melhor solução encontrada.
- **Orçamento de Tempo do TSA:** Limitado estritamente a `TIME_LIMIT = 1.0 - 1e-13` segundo subtraído do tempo já consumido pela heurística construtiva inicial.

---

## 🤖 Framework de Aprendizado por Reforço (SAC-TSA)

O módulo `rl/` modela a calibração de hiperparâmetros do **Timed Simulated Annealing (TSA)** como um **Contextual Bandit** de passo único ($\gamma = 0.0$), resolvido com **Soft Actor-Critic (SAC)** (`Stable-Baselines3`) e normalização de estado via `VecNormalize`.

### 1. Espaço de Observação Invariante à Escala (16 *Features*)

Calculado em C++ por `metrics::calculate_rl_state()` (`include/mmkp.hpp`), o vetor de 16 dimensões descreve a heterogeneidade das demandas e a pressão de carga em cada camada independentemente da escala absoluta da instância:

| Grupo | *Features* | Descrição |
| :--- | :--- | :--- |
| **Heterogeneidade das Demandas (CV)** | `cv_d_cpu`, `cv_d_mem`, `cv_d_sto`, `cv_d_cost` | Coeficientes de variação ($\sigma / \mu$) de CPU, MEM, STO e custo de penalidade dos dispositivos. |
| **Razões de Pressão na Borda (EC)** | `ratio_de_cpu`, `ratio_de_mem`, `ratio_de_sto`, `ratio_de_dps` | Demanda total dos dispositivos dividida pela capacidade total da Borda, além de dispositivos por servidor EC. |
| **Razões de Pressão na Nuvem (CC)** | `ratio_dc_cpu`, `ratio_dc_mem`, `ratio_dc_sto`, `ratio_dc_dps` | Demanda total dos dispositivos dividida pela capacidade total da Nuvem, além de dispositivos por servidor CC. |
| **Razões de Pressão no Sistema** | `ratio_ds_cpu`, `ratio_ds_mem`, `ratio_ds_sto`, `ratio_ds_dps` | Demanda total dos dispositivos dividida pela capacidade total (EC + CC), além de dispositivos por servidor total. |

### 2. Espaço de Ação Contínuo e Mapeamento de Hiperparâmetros

O SAC gera ações contínuas normalizadas $u \in [-1.0, 1.0]$, mapeadas linearmente por partes ao redor dos valores centrais de referência:
- **Temperatura Inicial ($T_0$):** Mapeada nos limites `(min=1.0, max=100.0, mid=50.0)`.
- **Taxa de Resfriamento ($\alpha$):** Parametrizada em escala logarítmica pelo expoente $k \in [1.0, 5.0]$ centrado em $k_{\text{mid}} = 3.0$, resultando em $\alpha = 1 - 10^{-k} \in [0.9, 0.99999]$ (centrado em $\alpha = 0.999$).

### 3. Função de Recompensa (Acoplamento Beta-Quadrático)

Após executar `num_runs = 30` rodadas independentes do TSA em C++ por episódio, o custo médio normalizado $\hat{C} = \text{clip}(C_{\text{méd}} / C_{\max}, 0, 1)$ e o tempo médio normalizado $\hat{T} = \text{clip}(T_{\text{méd}} / T_{\max}, 0, 1)$ compõem a recompensa escalar:

$$R = -\hat{C} \cdot \left[1 + \hat{C}(1 - \hat{C}) \cdot \hat{T}^2\right]$$

Essa formulação prioriza a minimização do custo de penalidade de SLA enquanto penaliza suavemente tempos de execução elevados nos regimes intermediários de custo.

### 4. Agentes SAC Treinados e *Baselines* (`rl/train.py` e `rl/evaluate.py`)

Seis agentes SAC especializados são treinados de forma incremental sobre os mesmos episódios compartilhados e avaliados frente a duas *baselines* TSA de hiperparâmetros fixos (`T = 50.0`, `alpha = 0.999`):

| Abordagem / Diretório do Agente | Modo (`Model_Type`) | Heurística Inicial (`option`) | Parâmetros Calibrados |
| :--- | :---: | :---: | :--- |
| `Baseline_1_TSA_GCB` | `NONE` | `GCB` (`5`) | Fixos ($T_0 = 50.0,\ \alpha = 0.999$) |
| `Baseline_2_TSA_BGR` | `NONE` | `BGR` (`8`) | Fixos ($T_0 = 50.0,\ \alpha = 0.999$) |
| `exp1_T_GCB` | `T` | `GCB` (`5`) | $T_0$ dinâmico ($\alpha = 0.999$ fixo) |
| `exp2_alpha_GCB` | `alpha` | `GCB` (`5`) | $\alpha$ dinâmico ($T_0 = 50.0$ fixo) |
| `exp3_both_GCB` | `both` | `GCB` (`5`) | $T_0$ e $\alpha$ calibrados conjuntamente |
| `exp4_T_BGR` | `T` | `BGR` (`8`) | $T_0$ dinâmico ($\alpha = 0.999$ fixo) |
| `exp5_alpha_BGR` | `alpha` | `BGR` (`8`) | $\alpha$ dinâmico ($T_0 = 50.0$ fixo) |
| `exp6_both_BGR` | `both` | `BGR` (`8`) | $T_0$ e $\alpha$ calibrados conjuntamente |

Cada agente persiste três artefatos em `rl/models/<agent_tag>/` para permitir retomada contínua do treinamento (`start_episode = "auto"`): `sac_model.zip`, `replay_buffer.pkl` e `vec_normalize.pkl`.

---

## 🔧 Parâmetros de Simulação e Treinamento

### Parâmetros do Benchmark C++ (`src/main.cpp`)

| Parâmetro (`SimulationConfig`) | Flag CLI | Padrão | Descrição |
| :--- | :--- | :---: | :--- |
| `services` | `--services` | `100` | Quantidade de perfis distintos em `Services.txt`. |
| `devices_start` / `step` / `end` | `--devices-start` / `-step` / `-end` | `15000` / `1000` / `20000` | Faixa de escalabilidade de dispositivos IoT nos lotes. |
| `ec_length` / `cc_length` | `--ec-length` / `--cc-length` | `499` / `1` | Quantidade de servidores de Borda (EC) e Nuvem (CC). |
| `max_runs` / `run_id` | `--max-runs` / `--run-id` | `50` / `0` | Rodadas independentes por instância (`run_id > 0` executa apenas aquele ID). |
| `cplex_time_limit` | `--cplex-time-limit` | `3600` | Tempo limite (*wall-clock*) do solver CPLEX em segundos (1 hora). |
| `temperature` / `alpha` | `--temperature` / `--alpha` | `50.0` / `0.9999` | Temperatura inicial e taxa de resfriamento para SA/TSA avulso. |
| `fixed_penalty` | `--fixed-penalty` | `false` | `true` fixa a penalidade em `1.0`; `false` usa custo proporcional às tarefas. |
| `option_start` / `option_end` | `--option-start` / `--option-end` | `0` / `3` | Intervalo inclusivo de opções de algoritmos executados. |
| `training` / `init_only` | `--training` / `--init-only` | `false` / `false` | Direciona I/O para `training/episode_<X>/` ou apenas imprime o estado RL. |

### Configurações do Solver Exato CPLEX (`include/exact.hpp`)

- `Threads = 1` (execução *single-threaded* para comparação determinística)
- `ClockType = 2` (tempo de relógio *wall-clock*)
- `TimeLimit = 3600` s (configurável via `--cplex-time-limit`)
- `WorkMem = 16384` MB (16 GB de memória de trabalho antes de gravar nós em disco)
- `MIP::Strategy::File = 3` (arquivos de nós compactados em disco)
- `MIP::Tolerances::MIPGap = 1e-3` (tolerância de *gap* relativo de 0,1%)
- `Emphasis::MIP = MIPEmphasisBalanced`

### Distribuições dos Datasets Sintéticos (`include/mmkp.hpp`)

Os datasets usam ponto e vírgula (`;`) como separador e são gerados automaticamente via `std::mt19937` caso não existam:

- **Serviços (`Services.txt`):** `TSK` $\sim \mathcal{U}\{1, 4\}$; para cada tarefa, acumulam-se demandas uniformes: `CPU` $\mathrel{+}= \mathcal{U}(10^{-13}, 2.5)$, `MEM` $\mathrel{+}= \mathcal{U}(10^{-13}, 2.5)$, `STO` $\mathrel{+}= \mathcal{U}(10^{-13}, 15.0)$ e `COST` $= 1.0 + \sum \mathcal{U}(10^{-13}, 2.5)$.
- **Dispositivos (`Devices_<N>.txt`):** Cada dispositivo sorteia uniformemente um perfil em `Services.txt` (com `COST` sobrescrito para `1.0` quando `fixed_penalty = true`).
- **Servidores de Borda (`'E'`):** `CPU` $\sim \mathcal{U}(2.5, 25.0)$, `MEM` $\sim \mathcal{U}(2.5, 125.0)$, `STO` $\sim \mathcal{U}(15.0, 1000.0)$.
- **Servidores de Nuvem (`'C'`):** `CPU` $\sim \mathcal{U}(50000, 100000)$, `MEM` $\sim \mathcal{U}(25600, 51400)$, `STO` $\sim \mathcal{U}(250000, 500000)$.

### Parâmetros de Treinamento e Avaliação RL (`rl/train.py` e `rl/evaluate.py`)

| Módulo | Parâmetro | Padrão | Descrição |
| :--- | :--- | :---: | :--- |
| **Treino (`train.py`)** | `start_episode` / `episodes_to_train` | `"auto"` / `100` | Janela incremental de episódios (detecta o próximo episódio local automaticamente). |
| **Treino (`train.py`)** | `num_runs_per_episode` | `30` | Quantidade de rodadas estatísticas de TSA (e BGR) por episódio. |
| **Treino (`train.py`)** | `devices_range` / `ec_range` / `cc_range` | `(15000, 25000)` / `(499, 499)` / `(1, 1)` | Limites sorteados para geração de cada episódio de treino. |
| **SAC (`train.py`)** | `learning_rate` / `batch_size` / `buffer_size` | `1e-3` / `32` / `50000` | Taxa de aprendizado, tamanho do lote e capacidade do *replay buffer*. |
| **SAC (`train.py`)** | `gamma` / `tau` / `ent_coef` / `learning_starts` | `0.0` / `0.005` / `"auto"` / `20` | Desconto *contextual bandit*, atualização suave, entropia automática e passos iniciais. |
| **SAC (`train.py`)** | `train_freq` / `gradient_steps` / `clip_obs` | `(1, "step")` / `1` / `10.0` | Frequência de atualização *online* e recorte das observações no `VecNormalize`. |
| **Avaliação (`evaluate.py`)** | `--devices` / `--ec` / `--cc` | `15000..20000` (passo `1000`) / `499` / `1` | Cenários avaliados nas 8 abordagens (`30` rodadas cada). |
| **Avaliação (`evaluate.py`)** | `--episodes` / `--model-file` | `None` (auto) / `"sac_model.zip"` | Identificador do checkpoint (`tsa_<episodes>.txt`) e arquivo do modelo. |
| **Avaliação (`evaluate.py`)** | `--devices-file` / `--servers-file` | `None` | Caminhos opcionais para extrair automaticamente as quantidades pelo nome do arquivo. |

---

## 📊 Resultados da Simulação e Artefatos (`results/`)

Todas as saídas tabulares são gravadas em arquivos delimitados por ponto e vírgula (`;`) com precisão de 10 casas decimais (`DECIMAL_PLACES = 10`):

- **`results/EXACT/SOLVER_MAXIMUM_COST.txt`:** Saídas do solver exato incluindo `Status`, `Obj_Function` e `Gap`.
- **`results/HEURISTIC/heuristics.txt` e `results/HEURISTIC/cache/`:** Métricas das heurísticas construtivas e *snapshots* binários das soluções iniciais (`Snapshots_D<N>_EC<EC>_CC<CC>_R<R>.bin`).
- **`results/METAHEURISTIC/metaheuristics.txt` e `tsa_<episodes>.txt`:** Resultados de benchmarks SA/TSA e arquivos de avaliação por checkpoint de RL contendo `Initial_Heuristic`, `Temperature`, `Alpha` e `Model_Type` (`NONE`, `T`, `alpha` ou `both`).
- **Modelos Pré-Treinados e Artefatos de Benchmark:** O repositório versiona os resultados do solver exato (`results/EXACT/`), métricas e caches binários das heurísticas construtivas (`results/HEURISTIC/`), resultados de SA/TSA e checkpoints de avaliação RL (`results/METAHEURISTIC/tsa_*.txt`), além dos pesos SAC pré-treinados, *replay buffers* e estatísticas de normalização (`rl/models/`).
- **Episódios Locais de Treinamento RL (`data/training/` e `results/training/`):** Devido ao elevado volume de dados gerado ao longo de milhares de episódios (incluindo caches `initial_snapshots.bin`, `heuristics.txt` e `metaheuristics.txt` por episódio), ambos os diretórios são ignorados no controle de versão pelo `.gitignore`. Eles são gerados automaticamente de forma local ao executar `python3 rl/train.py` e protegidos pelas flags de limpeza do `build.sh` (`-d`, `-r`, `-a`).
- **Lotes Históricos de Benchmark:** Nos arquivos raiz (`EXACT/`, `HEURISTIC/`, `METAHEURISTIC/`), o primeiro lote de execuções (`10.000` a `20.000` dispositivos) utilizou penalidade constante (`fixed_penalty = true`, custo `1.0`), enquanto os lotes subsequentes utilizaram penalidade variável proporcional às tarefas (`fixed_penalty = false`).

---

## 📚 Documentação (Doxygen)

O projeto utiliza Doxygen (com diagramas Graphviz, MathJax e exportação em PDF via LaTeX) configurado em `docs/Doxyfile`, com publicação automatizada no GitHub Pages por meio do workflow `.github/workflows/docs.yml`.

Para gerar ou atualizar localmente a documentação em HTML (`docs/html/index.html`) e em PDF (`docs/html/MMKP_Documentation.pdf` / `docs/latex/refman.pdf`), execute na raiz do projeto:

~~~bash
./build.sh --docs
~~~