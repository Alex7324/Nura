#include "eval.h"

#include <stdio.h>

/*
 * Bandierina di errore a runtime, valida per tutta una valutazione.
 * La teniamo a livello di file (static) cosi' la funzione ricorsiva non
 * deve trascinarsela come parametro a ogni chiamata. Viene azzerata
 * all'inizio di ogni eval_expression().
 */
static int had_runtime_error;

static void runtime_error(const char *message) {
    if (had_runtime_error) return;   /* segnala solo il primo */
    had_runtime_error = 1;
    fprintf(stderr, "Errore a runtime: %s\n", message);
}

/*
 * Il cuore della Fase 3: attraversamento POST-ORDER dell'albero.
 * Confrontala con ast_free(): stessa struttura ricorsiva (prima i figli,
 * poi il nodo), ma qui invece di liberare, calcoliamo e combiniamo.
 */
static double evaluate(Expr *expr) {
    switch (expr->type) {

        /* Foglia: il valore e' gia' li', niente da calcolare. */
        case EXPR_NUMBER:
            return expr->as.number.value;

        /* Un operando: calcolalo, poi applica l'operatore prefisso. */
        case EXPR_UNARY: {
            double right = evaluate(expr->as.unary.right);
            switch (expr->as.unary.op) {
                case TOKEN_MINUS: return -right;
                default:          return 0;  /* non dovrebbe capitare */
            }
        }

        /* Due operandi: calcola SINISTRO, poi DESTRO, poi combina.
         * Le due chiamate ricorsive qui sotto sono la valutazione dei
         * sottoalberi: e' cosi' che la precedenza (gia' nell'albero)
         * si traduce nell'ordine giusto dei calcoli. */
        case EXPR_BINARY: {
            double left  = evaluate(expr->as.binary.left);
            double right = evaluate(expr->as.binary.right);
            switch (expr->as.binary.op) {
                case TOKEN_PLUS:  return left + right;
                case TOKEN_MINUS: return left - right;
                case TOKEN_STAR:  return left * right;
                case TOKEN_SLASH:
                    if (right == 0) {
                        runtime_error("divisione per zero.");
                        return 0;
                    }
                    return left / right;

                /* Confronti: 1.0 = vero, 0.0 = falso (provvisorio). */
                case TOKEN_EQ:  return left == right ? 1.0 : 0.0;
                case TOKEN_NEQ: return left != right ? 1.0 : 0.0;
                case TOKEN_LT:  return left <  right ? 1.0 : 0.0;
                case TOKEN_LE:  return left <= right ? 1.0 : 0.0;
                case TOKEN_GT:  return left >  right ? 1.0 : 0.0;
                case TOKEN_GE:  return left >= right ? 1.0 : 0.0;

                default:        return 0;  /* non dovrebbe capitare */
            }
        }
    }
    return 0;  /* irraggiungibile: tutti i tipi sono gestiti sopra */
}

double eval_expression(Expr *expr, int *had_error) {
    had_runtime_error = 0;
    double result = evaluate(expr);
    *had_error = had_runtime_error;
    return result;
}
