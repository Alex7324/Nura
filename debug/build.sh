#!/bin/sh
# Compila gli strumenti di debug del progetto.
# Eseguire dalla cartella principale:  sh debug/build.sh

# -lm = libreria matematica (fmod), richiesta su Linux/Mac; innocua su Windows.

# 1) Narratore del FLUSSO (Fasi 1-3): espressione -> token -> albero -> valore.
gcc -std=gnu11 -I src \
    debug/flow_parser.c debug/flow_eval.c debug/flow_main.c \
    src/token.c src/lexer.c src/ast.c \
    -o nura_flow -lm
echo "Compilato: nura_flow   (prova:  ./nura_flow \"1 + 2 * 3\")"

# 2) Narratore dell'ESECUZIONE (Fase 4-5): programma -> istruzioni -> ambiente.
gcc -std=gnu11 -I src \
    debug/nura_run.c \
    src/parser.c src/ast.c src/lexer.c src/token.c src/env.c src/value.c src/gc.c \
    -o nura_run -lm
echo "Compilato: nura_run    (prova:  ./nura_run \"var n = 5; print n * 2;\")"

# 3) Visualizzatore dell'AMBIENTE (Fase 4): la tabella hash nome -> valore.
gcc -std=gnu11 -I src \
    debug/env_demo.c src/env.c src/value.c src/gc.c \
    -o env_demo -lm
echo "Compilato: env_demo    (prova:  ./env_demo)"
