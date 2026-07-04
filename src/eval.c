#include "eval.h"

#include <stdio.h>
#include <math.h>   /* fmod, per l'operatore modulo % */

static int had_runtime_error;

static void runtime_error(const char *message) {
    if (had_runtime_error) return;
    had_runtime_error = 1;
    fprintf(stderr, "Errore a runtime: %s\n", message);
}

/* ------------------------------------------------------------------ */
/*  Valutazione di un'espressione (ora con l'ambiente)                */
/* ------------------------------------------------------------------ */

static double evaluate(Expr *expr, Env *env) {
    switch (expr->type) {

        case EXPR_NUMBER:
            return expr->as.number.value;

        /* Lettura di una variabile: la cerchiamo nell'ambiente. */
        case EXPR_VARIABLE: {
            double value;
            if (!env_get(env, expr->as.variable.name, &value)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "variabile '%s' non definita.",
                         expr->as.variable.name);
                runtime_error(msg);
                return 0;
            }
            return value;
        }

        /* Assegnamento: calcola il valore e aggiorna la variabile ESISTENTE. */
        case EXPR_ASSIGN: {
            double value = evaluate(expr->as.assign.value, env);
            if (!env_assign(env, expr->as.assign.name, value)) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "assegnamento a variabile '%s' non definita.",
                         expr->as.assign.name);
                runtime_error(msg);
                return 0;
            }
            return value;   /* l'assegnamento vale il valore assegnato */
        }

        case EXPR_UNARY: {
            double right = evaluate(expr->as.unary.right, env);
            switch (expr->as.unary.op) {
                case TOKEN_MINUS: return -right;
                default:          return 0;
            }
        }

        case EXPR_BINARY: {
            double left  = evaluate(expr->as.binary.left, env);
            double right = evaluate(expr->as.binary.right, env);
            switch (expr->as.binary.op) {
                case TOKEN_PLUS:  return left + right;
                case TOKEN_MINUS: return left - right;
                case TOKEN_STAR:  return left * right;
                case TOKEN_SLASH:
                    if (right == 0) { runtime_error("divisione per zero."); return 0; }
                    return left / right;
                case TOKEN_PERCENT:
                    if (right == 0) { runtime_error("modulo per zero."); return 0; }
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
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Esecuzione di un'istruzione                                       */
/* ------------------------------------------------------------------ */

static void execute(Stmt *stmt, Env *env) {
    switch (stmt->type) {

        case STMT_VAR: {
            double value = evaluate(stmt->as.var.initializer, env);
            env_define(env, stmt->as.var.name, value);   /* crea o aggiorna */
            break;
        }

        case STMT_PRINT: {
            double value = evaluate(stmt->as.print.expr, env);
            if (!had_runtime_error) printf("%g\n", value);
            break;
        }

        case STMT_EXPR: {
            evaluate(stmt->as.expr.expr, env);   /* valuta per l'effetto (es. n = 1) */
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Esecuzione del programma intero                                   */
/* ------------------------------------------------------------------ */

void run_program(Program *program, Env *env, int *had_error) {
    had_runtime_error = 0;
    for (int i = 0; i < program->count; i++) {
        execute(program->statements[i], env);
        if (had_runtime_error) break;   /* al primo errore ci fermiamo */
    }
    *had_error = had_runtime_error;
}
