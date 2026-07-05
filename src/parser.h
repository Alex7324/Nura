#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int had_error;
    int depth;        /* complessita' dell'espressione corrente: guard-rail
                       * contro lo stack overflow su annidamenti profondi
                       * ([[[...]]], (((...))) e catene lunghe (1+1+...+1).
                       * Azzerato a ogni istruzione. Vedi MAX_PARSE_DEPTH. */
} Parser;

/* (Fasi 1-3) Analizza UNA espressione e ne ritorna l'albero.
 * Ancora usata dagli strumenti di debug (nura_flow). */
Expr *parse_expression_source(const char *source, int *had_error);

/* (Fase 4) Analizza un INTERO programma: una lista di istruzioni.
 * Riempie `program` (che viene inizializzato qui dentro). */
void parse_program(const char *source, Program *program, int *had_error);

#endif /* PARSER_H */
