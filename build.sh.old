#!/bin/bash

# Interrompe o script imediatamente se ocorrer algum erro
set -e

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

# Esvazia o conteúdo dentro da pasta data (caminho corrigido para a raiz)
if [ "$CLEAN_DATA" = true ]; then
    echo "=== Limpando o conteúdo da pasta data/ ==="
    if [ -d "data" ]; then
        find data -mindepth 1 -delete
    fi
fi

# Esvazia o conteúdo dentro da pasta results (caminho corrigido para a raiz)
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
cd build

echo "=== Configurando o projeto com o CMake ==="
cmake ..

echo "=== Compilando o projeto ==="
make -j$(nproc)

# Retorna para a raiz do projeto ANTES de executar o binário
cd ..

echo "=== Build concluído com sucesso! Executando a simulação... ==="
echo "------------------------------------------------------------"

# Executa o binário chamando-o a partir da raiz
./build/MMKP_IoT-EC-CC