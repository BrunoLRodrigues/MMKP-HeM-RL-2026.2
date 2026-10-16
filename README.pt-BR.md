# Alocação de Recursos MMKP em Redes IoT-EC-CC

[![Documentation HTML](https://img.shields.io/badge/docs-HTML%20(Doxygen)-blue?logo=read-the-docs)](https://<SEU_USUARIO>.github.io/<SEU_REPOSITORIO>/)
[![Documentation PDF](https://img.shields.io/badge/docs-PDF%20(LaTeX)-red?logo=adobe-acrobat-reader)](https://<SEU_USUARIO>.github.io/<SEU_REPOSITORIO>/MMKP_Documentation.pdf)

🌐 **Idioma / Language:** [English](README.md) | **Português (Brasil)**

Simulador em C++17 para resolução do **Problema das Múltiplas Mochilas Multidimensionais (MMKP)** aplicado à alocação de recursos em infraestruturas de **Internet das Coisas (IoT)**, **Computação de Borda (EC)** e **Computação em Nuvem (CC)**. O simulador avalia métodos Exatos (ILP via IBM ILOG CPLEX), Heurísticas Construtivas e Meta-heurísticas (*Simulated Annealing*).

---

## 📁 Estrutura do Projeto

~~~text
MMKP_IoT_EC_CC/
├── .github/workflows/
│   └── docs.yml            # Workflow do GitHub Actions para o Doxygen (HTML + PDF)
├── data/                   # Conjuntos de dados de entrada (gerados automaticamente se ausentes)
│   ├── devices/            # Demandas de dispositivos IoT (Devices_<N>.txt)
│   ├── servers/            # Capacidades dos servidores EC e CC (servers_<EC>_<CC>.txt)
│   └── Services.txt        # Perfis de serviços (COST, CPU, MEM, STO, TSK)
├── docs/
│   ├── html/               # Documentação HTML gerada pelo Doxygen (ignorada no Git)
│   ├── latex/              # Documentação LaTeX/PDF gerada pelo Doxygen (ignorada no Git)
│   └── Doxyfile            # Configuração de documentação do Doxygen
├── include/                # Implementação C++17 Header-Only
│   ├── exact.hpp           # Formulação matemática ILP no CPLEX
│   ├── heuristic.hpp       # Heurísticas construtivas (GR, GRF, GCB)
│   ├── metaheuristic.hpp   # Simulated Annealing e operadores de vizinhança
│   ├── mmkp.hpp            # Estruturas principais, leitura/escrita e métricas
│   └── tools.hpp           # Utilitários de CSV, números aleatórios, formatação e ordenação
├── results/                # Saídas das simulações (arquivos CSV e tabelas ASCII)
│   ├── EXACT/
│   ├── HEURISTIC/
│   └── METAHEURISTIC/
├── src/
│   └── main.cpp            # Ponto de entrada e parâmetros dos experimentos
├── build.sh                # Script automatizado de compilação e execução
├── CMakeLists.txt          # Configuração de build via CMake
├── README.md               # Documentação do projeto (Inglês)
└── README.pt-BR.md         # Documentação do projeto (Português)
~~~

## ⚙️ Pré-requisitos

* **Sistema Operacional:** Linux ou Windows Subsystem for Linux (WSL2 - Ubuntu 24.04 recomendado)
* **Compilador:** `g++` com suporte a C++17
* **Sistema de Build:** Bash (`build.sh`) ou `CMake` ($\ge 3.10$)
* **Solver:** IBM ILOG CPLEX Optimization Studio 22.2 instalado em:
  `/opt/ibm/ILOG/CPLEX_Studio222`

> **Nota:** Caso o seu CPLEX esteja instalado em outro diretório, atualize as variáveis `CPLEX_INCLUDE`, `CONCERT_INCLUDE`, `CPLEX_LIB` e `CONCERT_LIB` nos arquivos `build.sh` e `CMakeLists.txt`.

## 🚀 Como Compilar e Executar

### Opção 1: Usando o script `build.sh` (Recomendado)

Conceda permissão de execução e rode o script a partir da raiz do projeto:

~~~bash
chmod +x build.sh
./build.sh
~~~

O script aceita parâmetros opcionais antes de compilar e executar:
* `./build.sh -b` ou `--build`: Limpa os binários antigos da pasta `build/`.
* `./build.sh -d` ou `--data`: Apaga os arquivos da pasta `data/` (força a geração de novas instâncias).
* `./build.sh -r` ou `--results`: Limpa os resultados anteriores da pasta `results/`.
* `./build.sh -a` ou `--all`: Limpa `build/`, `data/` e `results/` simultaneamente.
* `./build.sh -doc` ou `--docs`: Regera a documentação HTML e PDF (LaTeX) do Doxygen.

### Opção 2: Usando CMake

~~~bash
mkdir -p build && cd build
cmake ..
make
cd ..
./build/MMKP_IoT-EC-CC
~~~
*(Execute sempre o binário a partir da pasta raiz do projeto para que os caminhos relativos de `data/` e `results/` funcionem corretamente).*

## 🧠 Algoritmos Implementados

| Opção (`main.cpp`) | Paradigma | Algoritmo | Descrição |
| :---: | :--- | :--- | :--- |
| `0` | `EXACT` | `SOLVER_MAXIMUM_COST` | Modelo exato em ILP resolvido via CPLEX maximizando as penalidades evitadas. |
| `-1` | `HEURISTIC` | `GREEDY_CB` | Alocação gulosa determinística ordenada pela razão custo-benefício multidimensional. |
| `-2` | `HEURISTIC` | `GREEDY_RANDOM` | *Baseline* estocástica que tenta alocar cada dispositivo em um servidor sorteado. |
| `-3` | `HEURISTIC` | `GREEDY_RANDOM_FALLBACK` | Alocação estocástica em dois estágios priorizando a Borda (EC) com *fallback* para Nuvem (CC). |
| `1` | `METAHEURISTIC` | `SA` + `GREEDY_CB` | *Simulated Annealing* inicializado com a solução da `GREEDY_CB`. |
| `2` | `METAHEURISTIC` | `SA` + `GREEDY_RANDOM` | *Simulated Annealing* inicializado com a solução da `GREEDY_RANDOM`. |
| `3` | `METAHEURISTIC` | `SA` + `GREEDY_RANDOM_FALLBACK` | *Simulated Annealing* inicializado com a solução da `GREEDY_RANDOM_FALLBACK`. |

*Nota: Ao executar as opções `1`, `2` e `3`, a solução inicial gerada pela heurística base já é automaticamente avaliada e salva em `results/HEURISTIC/` antes do início do Simulated Annealing.*

## 🔧 Parâmetros da Simulação

Os parâmetros do experimento podem ser ajustados diretamente em `src/main.cpp`:

* `SERVICES = 100`: Quantidade de perfis de serviços.
* `DEVICES_START = 10000`, `DEVICES_STEP = 1000`, `DEVICES_END = 20000`: Escala de dispositivos IoT.
* `EC_LENGTH = 499`, `CC_LENGTH = 1`: Quantidade de servidores de Borda (EC) e Nuvem (CC).
* `MAX_RUNS = 50`: Número de execuções independentes por instância.
* `CPLEX_TIME_LIMIT = 3600`: Tempo limite do CPLEX em segundos (1 hora).
* `TEMPERATURE = 50.0`, `ALPHA = 0.9999`: Parâmetros de resfriamento do *Simulated Annealing*.

## 📚 Documentação (Doxygen)

Para gerar ou atualizar localmente a documentação em HTML (`docs/html/index.html`) e em PDF (`docs/latex/refman.pdf`), execute na raiz do projeto:

~~~bash
doxygen docs/Doxyfile
make -C docs/latex
~~~