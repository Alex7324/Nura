#!/bin/sh
# Compila gli strumenti di debug del progetto.
# Eseguire dalla cartella principale:  sh debug/build.sh

# 1) Narratore del FLUSSO (Fasi 1-3): espressione -> token -> albero -> valore.
gcc -std=c11 -I src \
    debug/flow_parser.c debug/flow_eval.c debug/flow_main.c \
    src/token.c src/lexer.c src/ast.c \
    -o nura_flow
echo "Compilato: nura_flow   (prova:  ./nura_flow \"1 + 2 * 3\")"

# 2) Visualizzatore dell'AMBIENTE (Fase 4): la tabella hash nome -> valore.
gcc -std=c11 -I src \
    debug/env_demo.c src/env.c \
    -o env_demo
echo "Compilato: env_demo    (prova:  ./env_demo)"
