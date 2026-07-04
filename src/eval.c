#include "eval.h"

#include <stdio.h>
#include <math.h>   /* fmod, per l'operatore modulo % */

/*
 * Il "contesto" dell'esecuzione: raccoglie tutto lo stato che serve mentre
 * il programma gira — l'ambiente delle variabili e la bandierina di errore.
 *
 * Lo passiamo esplicitamente a ogni funzione (esattamente come il Parser
 * passa in giro il suo stato), invece di usare variabili globali. Cosi':
 *   - le firme delle funzioni dicono la verita' su cosa toccano;
 *   - niente stato nascosto: l'errore viaggia in modo visibile;
 *   - e' "rientrante": potrebbero coesistere piu' esecuzioni indipendenti.
 */
typedef struct {
    Env *env;
    int had_error;
} Interp;

static void runtime_error(Interp *it, const char *message) {
    if (it->had_error) return;   /* segnaliamo solo il primo errore */
    it->had_error = 1;
    fprintf(stderr, "Errore a runtime: %s\n", message);
}

/* Regola di verita': 0 e' falso, qualsiasi altro numero e' vero.
 * Serve a if e while per decidere, e all'operatore ! per negare. */
static int is_truthy(double value) {
    return value != 0;
}

/* ------------------------------------------------------------------ */
/*  Valutazione di un'espressione                                     */
/* ------------------------------------------------------------------ */

static double evaluate(Expr *expr, Interp *it) {
    switch (expr->type) {

        case EXPR_NUMBER:
            return expr->as.number.value;

        case EXPR_VARIABLE: {
            double value;
            if (!env_get(it->env, expr->as.variable.name, &value)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "variabile '%s' non definita.",
                         expr->as.variable.name);
                runtime_error(it, msg);
                return 0;
            }
            return value;
        }

        case EXPR_ASSIGN: {
            double value = evaluate(expr->as.assign.value, it);
            if (!env_assign(it->env, expr->as.assign.name, value)) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "assegnamento a variabile '%s' non definita.",
                         expr->as.assign.name);
                runtime_error(it, msg);
                return 0;
            }
            return value;
        }

        case EXPR_UNARY: {
            double right = evaluate(expr->as.unary.right, it);
            switch (expr->as.unary.op) {
                case TOKEN_MINUS: return -right;
                case TOKEN_BANG:
                    if (is_truthy(right)) return 0.0;   /* !vero  = falso */
                    else                  return 1.0;   /* !falso = vero  */
                default:          return 0;
            }
        }

        case EXPR_BINARY: {
            double left  = evaluate(expr->as.binary.left, it);
            double right = evaluate(expr->as.binary.right, it);
            switch (expr->as.binary.op) {
                case TOKEN_PLUS:  return left + right;
                case TOKEN_MINUS: return left - right;
                case TOKEN_STAR:  return left * right;
                case TOKEN_SLASH:
                    if (right == 0) { runtime_error(it, "divisione per zero."); return 0; }
                    return left / right;
                case TOKEN_PERCENT:
                    if (right == 0) { runtime_error(it, "modulo per zero."); return 0; }
                    return fmod(left, right);

                /* Confronti: 1.0 = vero, 0.0 = falso (provvisorio). */
                case TOKEN_EQ:
                    if (left == right) return 1.0;
                    else               return 0.0;
                case TOKEN_NEQ:
                    if (left != right) return 1.0;
                    else               return 0.0;
                case TOKEN_LT:
                    if (left < right) return 1.0;
                    else              return 0.0;
                case TOKEN_LE:
                    if (left <= right) return 1.0;
                    else               return 0.0;
                case TOKEN_GT:
                    if (left > right) return 1.0;
                    else              return 0.0;
                case TOKEN_GE:
                    if (left >= right) return 1.0;
                    else               return 0.0;

                default:        return 0;
            }
        }

        case EXPR_LOGICAL: {
            /* CORTO CIRCUITO: valutiamo il sinistro, e il destro SOLO se serve.
             *   falso && ...  -> gia' falso, non guardo il destro
             *   vero  || ...  -> gia' vero,  non guardo il destro         */
            double left = evaluate(expr->as.logical.left, it);
            if (it->had_error) return 0;

            if (expr->as.logical.op == TOKEN_OR) {
                if (is_truthy(left)) return 1.0;
            } else { /* TOKEN_AND */
                if (!is_truthy(left)) return 0.0;
            }

            /* Il risultato dipende dal destro. */
            double right = evaluate(expr->as.logical.right, it);
            if (is_truthy(right)) return 1.0;
            else                  return 0.0;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Esecuzione di un'istruzione                                       */
/* ------------------------------------------------------------------ */

static void execute(Stmt *stmt, Interp *it) {
    switch (stmt->type) {

        case STMT_VAR: {
            double value = evaluate(stmt->as.var.initializer, it);
            env_define(it->env, stmt->as.var.name, value);
            break;
        }

        case STMT_PRINT: {
            double value = evaluate(stmt->as.print.expr, it);
            if (!it->had_error) printf("%g\n", value);
            break;
        }

        case STMT_EXPR: {
            evaluate(stmt->as.expr.expr, it);
            break;
        }

        case STMT_BLOCK: {
            /* esegue in ordine le istruzioni del blocco */
            Program *body = &stmt->as.block.body;
            for (int i = 0; i < body->count; i++) {
                execute(body->statements[i], it);
                if (it->had_error) break;
            }
            break;
        }

        case STMT_IF: {
            double cond = evaluate(stmt->as.if_stmt.condition, it);
            if (it->had_error) break;
            if (is_truthy(cond)) {
                execute(stmt->as.if_stmt.then_branch, it);
            } else if (stmt->as.if_stmt.else_branch != NULL) {
                execute(stmt->as.if_stmt.else_branch, it);
            }
            break;
        }

        case STMT_WHILE: {
            /* ripete finche' la condizione resta vera */
            for (;;) {
                double cond = evaluate(stmt->as.while_stmt.condition, it);
                if (it->had_error) break;
                if (!is_truthy(cond)) break;      /* condizione falsa: esci */
                execute(stmt->as.while_stmt.body, it);
                if (it->had_error) break;
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Esecuzione del programma intero                                   */
/* ------------------------------------------------------------------ */

void run_program(Program *program, Env *env, int *had_error) {
    Interp it;
    it.env = env;
    it.had_error = 0;

    for (int i = 0; i < program->count; i++) {
        execute(program->statements[i], &it);
        if (it.had_error) break;   /* al primo errore ci fermiamo */
    }

    *had_error = it.had_error;
}
