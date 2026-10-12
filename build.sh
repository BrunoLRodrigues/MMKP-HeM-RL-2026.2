#!/bin/bash

# Interrompe o script imediatamente se ocorrer algum erro
set -e

# Configurações do CPLEX IBM (Baseado no tasks.json)
CPLEX_INCLUDE="/opt/ibm/ILOG/CPLEX_Studio126/cplex/include"
CONCERT_INCLUDE="/opt/ibm/ILOG/CPLEX_Studio126/concert/include"
CPLEX_LIB="/opt/ibm/ILOG/CPLEX_Studio126/cplex/lib/x86-64_linux/static_pic"
CONCERT_LIB="/opt/ibm/ILOG/CPLEX_Studio126/concert/lib/x86-64_linux/static_pic"

# Variáveis de controle para limpeza (false por padrão)
CLEAN_DATA=false
CLEAN_RESULTS=false
CLEAN_BUILD=false

# Processa os argumentos passados no terminal
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d|--data) CLEAN_DATA=true ;;
        -r|--results) CLEAN_RESULTS=true ;;
        -b|--build) CLEAN_BUILD=true ;;
        -a|--all) CLEAN_DATA=true; CLEAN_RESULTS=true; CLEAN_BUILD=true ;;
        *) echo "Parâmetro desconhecido: $1"; exit 1 ;;
    esac
    shift
done

# Esvazia o conteúdo dentro da pasta data
if [ "$CLEAN_DATA" = true ]; then
    echo "=== Limpando o conteúdo da pasta data/ ==="
    if [ -d "data" ]; then
        find data -mindepth 1 -delete
    fi
fi

# Esvazia o conteúdo dentro da pasta results
if [ "$CLEAN_RESULTS" = true ]; then
    echo "=== Limpando o conteúdo da pasta results/ ==="
    if [ -d "results" ]; then
        find results -mindepth 1 -delete
    fi
fi

# Limpa o conteúdo da pasta build
if [ "$CLEAN_BUILD" = true ]; then
    echo "=== Removendo arquivos de build antigos ==="
    rm -rf build/*
fi

echo "=== Configurando o diretório build ==="
mkdir -p build

echo "=== Compilando o projeto com G++ (CPLEX + Heuristics) ==="
# Executa a linha de compilação direta do G++ espelhando o formato do tasks.json
/usr/bin/g++ -O -fPIC -fexceptions -DNDEBUG -DIL_STD \
    -I$CPLEX_INCLUDE -I$CONCERT_INCLUDE -Iinclude \
    -g src/*.cpp -o build/solver_model \
    -L$CPLEX_LIB -lilocplex -lcplex \
    -L$CONCERT_LIB -lconcert -lm -lpthread -w

echo "=== Build concluído com sucesso! Executando a simulação... ==="
echo "------------------------------------------------------------"

# Executa o binário final a partir da raiz (mantendo o Current Working Directory correto)
./build/solver_model