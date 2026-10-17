#!/bin/bash

# =============================================================================
# EN: Stop the script immediately if any command fails
# PT: Interrompe o script imediatamente se ocorrer algum erro
# =============================================================================
set -e

# =============================================================================
# EN: IBM ILOG CPLEX 22.2 paths (matching tasks.json and CMakeLists.txt)
# PT: Caminhos do IBM ILOG CPLEX 22.2 (baseado no tasks.json e CMakeLists.txt)
# =============================================================================
CPLEX_INCLUDE="/opt/ibm/ILOG/CPLEX_Studio222/cplex/include"
CONCERT_INCLUDE="/opt/ibm/ILOG/CPLEX_Studio222/concert/include"
CPLEX_LIB="/opt/ibm/ILOG/CPLEX_Studio222/cplex/lib/x86-64_linux/static_pic"
CONCERT_LIB="/opt/ibm/ILOG/CPLEX_Studio222/concert/lib/x86-64_linux/static_pic"

# =============================================================================
# EN: Control flags for cleanup and documentation (false by default)
# PT: Variáveis de controle para limpeza e documentação (false por padrão)
# =============================================================================
CLEAN_DATA=false
CLEAN_RESULTS=false
CLEAN_BUILD=false
GEN_DOCS=false

# =============================================================================
# EN: Parse command-line arguments
# PT: Processa os argumentos passados no terminal
# =============================================================================
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d|--data)    CLEAN_DATA=true ;;
        -r|--results) CLEAN_RESULTS=true ;;
        -b|--build)   CLEAN_BUILD=true ;;
        -a|--all)     CLEAN_DATA=true; CLEAN_RESULTS=true; CLEAN_BUILD=true ;;
        -doc|--docs)  GEN_DOCS=true ;;
        *) echo "Unknown parameter / Parâmetro desconhecido: $1"; exit 1 ;;
    esac
    shift
done

# EN: Empty the data/ directory if requested
# PT: Esvazia o conteúdo dentro da pasta data/ se solicitado
if [ "$CLEAN_DATA" = true ]; then
    echo "=== [EN] Cleaning data/ directory | [PT] Limpando a pasta data/ ==="
    if [ -d "data" ]; then
        find data -mindepth 1 -delete
    fi
fi

# EN: Empty the results/ directory if requested
# PT: Esvazia o conteúdo dentro da pasta results/ se solicitado
if [ "$CLEAN_RESULTS" = true ]; then
    echo "=== [EN] Cleaning results/ directory | [PT] Limpando a pasta results/ ==="
    if [ -d "results" ]; then
        find results -mindepth 1 -delete
    fi
fi

# EN: Clean the build/ directory if requested
# PT: Limpa o conteúdo da pasta build/ se solicitado
if [ "$CLEAN_BUILD" = true ]; then
    echo "=== [EN] Removing old build files | [PT] Removendo arquivos de build antigos ==="
    rm -rf build/*
fi

# EN: Generate Doxygen documentation (HTML + LaTeX PDF) if requested
# PT: Gera a documentação Doxygen (HTML + PDF LaTeX) se solicitado
if [ "$GEN_DOCS" = true ]; then
    echo "=== [EN] Generating Doxygen docs | [PT] Gerando documentação Doxygen ==="
    cp README.md README.md.tmp
    sed -i '/img.shields.io/d' README.md
    doxygen docs/Doxyfile
    mv README.md.tmp README.md

    if [ -d "docs/latex" ]; then
        sed -i 's/\\UTF{[0-9A-Fa-f]*}//g' docs/latex/*.tex 2>/dev/null || true
        (
            cd docs/latex
            pdflatex -interaction=nonstopmode refman.tex >/dev/null || true
            makeindex refman.idx >/dev/null 2>&1 || true
            pdflatex -interaction=nonstopmode refman.tex >/dev/null || true
            pdflatex -interaction=nonstopmode refman.tex >/dev/null || true
        )
        if [ -f "docs/latex/refman.pdf" ]; then
            cp docs/latex/refman.pdf docs/html/MMKP_Documentation.pdf
        fi
    fi
    # Encerra o script aqui se nenhuma flag de build/limpeza foi usada junto
    if [ "$CLEAN_BUILD" = false ] && [ "$CLEAN_DATA" = false ] && [ "$CLEAN_RESULTS" = false ]; then
        echo "=== [EN] Documentation generated successfully! | [PT] Documentação gerada com sucesso! ==="
        exit 0
    fi
fi

echo "=== [EN] Setting up build directory | [PT] Configurando o diretório build ==="
mkdir -p build

echo "=== [EN] Compiling with G++ (CPLEX + Heuristics) | [PT] Compilando com G++ ==="
# EN: Direct G++ compilation command with maximum optimization (-O3)
# PT: Linha de compilação direta do G++ com otimização máxima (-O3)
/usr/bin/g++ -O3 -std=c++17 -fPIC -fexceptions -DNDEBUG -DIL_STD \
    -I$CPLEX_INCLUDE -I$CONCERT_INCLUDE -Iinclude \
    src/*.cpp -o build/MMKP_IoT-EC-CC \
    -L$CPLEX_LIB -L$CONCERT_LIB \
    -lilocplex -lconcert -lcplex -lm -lpthread -ldl -w

echo "=== [EN] Build succeeded! Running simulation... | [PT] Build concluído! Executando... ==="
echo "-----------------------------------------------------------------------------------------"

# EN: Run the binary from the project root to preserve relative paths
# PT: Executa o binário a partir da raiz mantendo os caminhos relativos corretos
./build/MMKP_IoT-EC-CC