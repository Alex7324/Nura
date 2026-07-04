#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "env.h"

/* (Fasi 1-3) Valuta una singola espressione. Usata dagli strumenti di debug. */
double eval_expression(Expr *expr, int *had_error);

/* (Fase 4) Esegue un intero programma nell'ambiente `env`, eseguendo le
 * istruzioni in ordine. Mette *had_error a 1 se c'e' un errore a runtime. */
void run_program(Program *program, Env *env, int *had_error);

#endif /* EVAL_H */
