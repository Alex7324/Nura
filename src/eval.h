#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

/*
 * Valuta un albero di espressione e ne ritorna il valore numerico.
 *
 * In questa fase ogni valore e' un semplice `double` (siamo una
 * calcolatrice). I confronti (<, ==, ...) danno 1.0 per "vero" e 0.0 per
 * "falso": e' provvisorio, avremo un vero tipo booleano piu' avanti.
 *
 * Se durante il calcolo succede un errore a RUNTIME (per ora: divisione
 * per zero), stampa un messaggio e mette *had_error a 1.
 */
double eval_expression(Expr *expr, int *had_error);

#endif /* EVAL_H */
