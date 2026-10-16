# =============================================================================
# EN: Minimum CMake version and project name
# PT: Versão mínima do CMake e nome do projeto
# =============================================================================
cmake_minimum_required(VERSION 3.10)
project(MMKP_IoT-EC-CC)

# =============================================================================
# EN: Set the C++ standard to C++17
# PT: Define o padrão do C++ para C++17
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# 1. CPLEX CONFIGURATION / CONFIGURAÇÃO DO CPLEX (Studio 22.2)
# =============================================================================
# EN: Header directories (.h)
# PT: Diretórios de cabeçalhos (.h)
set(CPLEX_INCLUDE_DIR "/opt/ibm/ILOG/CPLEX_Studio222/cplex/include")
set(CONCERT_INCLUDE_DIR "/opt/ibm/ILOG/CPLEX_Studio222/concert/include")

# EN: Static library directories (.a)
# PT: Diretórios de bibliotecas estáticas (.a)
set(CPLEX_LIB_DIR "/opt/ibm/ILOG/CPLEX_Studio222/cplex/lib/x86-64_linux/static_pic")
set(CONCERT_LIB_DIR "/opt/ibm/ILOG/CPLEX_Studio222/concert/lib/x86-64_linux/static_pic")

# =============================================================================
# 2. DEPENDENCIES & TARGET / DEPENDÊNCIAS E EXECUTÁVEL
# =============================================================================
# EN: Find the POSIX Threads library (-lpthread)
# PT: Localiza a biblioteca Threads (-lpthread)
find_package(Threads REQUIRED)

# EN: Add the main executable target
# PT: Define o executável principal do projeto
add_executable(${PROJECT_NAME} src/main.cpp)

# =============================================================================
# 3. INCLUDE & LIBRARY PATHS / CAMINHOS DE INCLUSÃO E BIBLIOTECAS
# =============================================================================
# EN: Add project and CPLEX include directories
# PT: Adiciona os diretórios de cabeçalhos do projeto e do CPLEX
target_include_directories(${PROJECT_NAME} PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${CPLEX_INCLUDE_DIR}
    ${CONCERT_INCLUDE_DIR}
)

# EN: Add CPLEX library directories
# PT: Adiciona os diretórios de bibliotecas do CPLEX
target_link_directories(${PROJECT_NAME} PRIVATE 
    ${CPLEX_LIB_DIR}
    ${CONCERT_LIB_DIR}
)

# =============================================================================
# 4. LINKING & COMPILER FLAGS / LINKEDIÇÃO E FLAGS DE COMPILAÇÃO
# =============================================================================
# EN: Link libraries in the required static resolution order
# PT: Vincula as bibliotecas na ordem exigida para resolução estática
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ilocplex 
    concert 
    cplex 
    m 
    dl
    Threads::Threads
)

# EN: Add preprocessor definitions (-DNDEBUG -DIL_STD)
# PT: Adiciona definições de pré-processador (-DNDEBUG -DIL_STD)
target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG IL_STD)

# EN: Add optimization (-O3) and compiler flags
# PT: Adiciona flags de otimização máxima (-O3) e compilação
target_compile_options(${PROJECT_NAME} PRIVATE -O3 -fPIC -w -fexceptions)