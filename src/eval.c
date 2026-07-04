#include "eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Contesto dell'esecuzione:
 *   - env: l'ambiente delle variabili
 *   - had_error: bandierina di errore a runtime
 *   - strings/str_count/str_cap: "arena" delle stringhe create a runtime
 *     (es. dalle concatenazioni), liberate tutte alla fine del programma.
 */
typedef struct {
    Env *env;          /* scope corrente */
    Env *globals;      /* scope globale (i corpi delle funzioni lo racchiudono) */
    int had_error;
    int is_returning;  /* 1 mentre un 'return' sta "risalendo" fuori dalla funzione */
    Value return_value;
    char **strings;
    int str_count;
    int str_cap;
} Interp;

/* evaluate chiama execute (per le chiamate di funzione) e viceversa:
 * dichiarazione anticipata. */
static void execute(Stmt *stmt, Interp *it);

static void runtime_error(Interp *it, const char *message) {
    if (it->had_error) return;
    it->had_error = 1;
    fprintf(stderr, "Errore a runtime: %s\n", message);
}

/* Registra una stringa allocata a runtime, per liberarla a fine programma. */
static char *intern(Interp *it, char *s) {
    if (it->str_count == it->str_cap) {
        int new_cap;
        if (it->str_cap < 8) new_cap = 8;
        else                 new_cap = it->str_cap * 2;
        char **grown = realloc(it->strings, sizeof(char *) * (size_t)new_cap);
        if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
        it->strings = grown;
        it->str_cap = new_cap;
    }
    it->strings[it->str_count++] = s;
    return s;
}

/* Verifica che un valore sia un numero; altrimenti segnala errore. */
static int require_number(Interp *it, Value v, const char *what) {
    if (v.type == VAL_NUMBER) return 1;
    char msg[128];
    snprintf(msg, sizeof(msg), "%s richiede un numero, ma il valore e' di tipo %s.",
             what, value_type_name(v.type));
    runtime_error(it, msg);
    return 0;
}

/* Concatena due stringhe in una nuova (registrata nell'arena). */
static Value concat(Interp *it, const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *s = malloc(la + lb + 1);
    if (s == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(s, a, la);
    memcpy(s + la, b, lb + 1);   /* copia anche il '\0' di b */
    intern(it, s);
    return value_string(s);
}

/* Regola di verita' condivisa (delega a value.c). */
static int is_truthy(Value v) { return value_is_truthy(v); }

/* ------------------------------------------------------------------ */
/*  Valutazione di un'espressione -> Value                            */
/* ------------------------------------------------------------------ */

static Value evaluate(Expr *expr, Interp *it) {
    switch (expr->type) {

        case EXPR_NUMBER: return value_number(expr->as.number.value);
        case EXPR_BOOL:   return value_bool(expr->as.boolean.value);
        case EXPR_STRING: return value_string(expr->as.string.value);  /* punta all'AST */

        case EXPR_VARIABLE: {
            Value v;
            if (!env_get(it->env, expr->as.variable.name, &v)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "variabile '%s' non definita.",
                         expr->as.variable.name);
                runtime_error(it, msg);
                return value_number(0);
            }
            return v;
        }

        case EXPR_ASSIGN: {
            Value v = evaluate(expr->as.assign.value, it);
            if (it->had_error) return v;
            if (!env_assign(it->env, expr->as.assign.name, v)) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "assegnamento a variabile '%s' non definita.",
                         expr->as.assign.name);
                runtime_error(it, msg);
                return value_number(0);
            }
            return v;
        }

        case EXPR_UNARY: {
            Value r = evaluate(expr->as.unary.right, it);
            if (it->had_error) return r;
            if (expr->as.unary.op == TOKEN_BANG) {
                return value_bool(!is_truthy(r));
            }
            /* TOKEN_MINUS */
            if (!require_number(it, r, "il meno unario '-'")) return value_number(0);
            return value_number(-r.as.number);
        }

        case EXPR_BINARY: {
            Value l = evaluate(expr->as.binary.left, it);
            if (it->had_error) return l;
            Value r = evaluate(expr->as.binary.right, it);
            if (it->had_error) return r;
            TokenType op = expr->as.binary.op;

            /* Uguaglianza: funziona su qualsiasi tipo. */
            if (op == TOKEN_EQ)  return value_bool(values_equal(l, r));
            if (op == TOKEN_NEQ) return value_bool(!values_equal(l, r));

            /* '+' : somma di numeri OPPURE concatenazione di stringhe. */
            if (op == TOKEN_PLUS) {
                if (l.type == VAL_NUMBER && r.type == VAL_NUMBER)
                    return value_number(l.as.number + r.as.number);
                if (l.type == VAL_STRING && r.type == VAL_STRING)
                    return concat(it, l.as.string, r.as.string);
                runtime_error(it, "'+' vuole due numeri o due stringhe.");
                return value_number(0);
            }

            /* Gli altri operatori (- * / %  <  <= > >=) vogliono due numeri. */
            if (!require_number(it, l, "l'operatore") ||
                !require_number(it, r, "l'operatore")) {
                return value_number(0);
            }
            double a = l.as.number;
            double b = r.as.number;
            switch (op) {
                case TOKEN_MINUS: return value_number(a - b);
                case TOKEN_STAR:  return value_number(a * b);
                case TOKEN_SLASH:
                    if (b == 0) { runtime_error(it, "divisione per zero."); return value_number(0); }
                    return value_number(a / b);
                case TOKEN_PERCENT:
                    if (b == 0) { runtime_error(it, "modulo per zero."); return value_number(0); }
                    return value_number(fmod(a, b));
                case TOKEN_LT: return value_bool(a <  b);
                case TOKEN_LE: return value_bool(a <= b);
                case TOKEN_GT: return value_bool(a >  b);
                case TOKEN_GE: return value_bool(a >= b);
                default:       return value_number(0);
            }
        }

        case EXPR_LOGICAL: {
            /* Corto circuito: valutiamo il sinistro, il destro solo se serve. */
            Value l = evaluate(expr->as.logical.left, it);
            if (it->had_error) return l;

            if (expr->as.logical.op == TOKEN_OR) {
                if (is_truthy(l)) return value_bool(1);
            } else { /* TOKEN_AND */
                if (!is_truthy(l)) return value_bool(0);
            }

            Value r = evaluate(expr->as.logical.right, it);
            if (it->had_error) return r;
            return value_bool(is_truthy(r));
        }

        case EXPR_CALL: {
            Value callee = evaluate(expr->as.call.callee, it);
            if (it->had_error) return callee;
            if (callee.type != VAL_FUNCTION) {
                runtime_error(it, "si possono chiamare solo le funzioni.");
                return value_number(0);
            }

            Stmt *decl = callee.as.function;
            int argc = expr->as.call.arg_count;
            int paramc = decl->as.function.param_count;
            if (argc != paramc) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "la funzione '%s' vuole %d argomenti, ne hai passati %d.",
                         decl->as.function.name, paramc, argc);
                runtime_error(it, msg);
                return value_number(0);
            }

            /* Nuovo scope per la chiamata: racchiude il GLOBALE (scope lessicale),
             * cosi' la funzione vede se stessa (ricorsione) e le variabili globali,
             * ma NON le variabili locali del chiamante. */
            Env call_env;
            env_init(&call_env);
            call_env.enclosing = it->globals;

            /* Valuta gli argomenti nello scope del chiamante, legali ai parametri. */
            for (int i = 0; i < paramc; i++) {
                Value arg = evaluate(expr->as.call.args[i], it);
                if (it->had_error) { env_free(&call_env); return value_number(0); }
                env_define(&call_env, decl->as.function.params[i], arg);
            }

            /* Esegui il corpo nello scope della chiamata. */
            Env *saved = it->env;
            it->env = &call_env;
            execute(decl->as.function.body, it);
            it->env = saved;

            /* Recupera il valore di ritorno (0 se la funzione non fa 'return'). */
            Value result;
            if (it->is_returning) {
                result = it->return_value;
                it->is_returning = 0;
            } else {
                result = value_number(0);
            }

            /* Se ritorniamo una stringa che "vive" nel call_env, salviamola
             * nell'arena prima di distruggere lo scope. */
            if (result.type == VAL_STRING) {
                size_t len = strlen(result.as.string) + 1;
                char *copy = malloc(len);
                if (copy == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
                memcpy(copy, result.as.string, len);
                intern(it, copy);
                result = value_string(copy);
            }

            env_free(&call_env);
            return result;
        }
    }
    return value_number(0);
}

/* ------------------------------------------------------------------ */
/*  Esecuzione di un'istruzione                                       */
/* ------------------------------------------------------------------ */

static void execute(Stmt *stmt, Interp *it) {
    switch (stmt->type) {

        case STMT_VAR: {
            Value v = evaluate(stmt->as.var.initializer, it);
            if (!it->had_error) env_define(it->env, stmt->as.var.name, v);
            break;
        }

        case STMT_PRINT: {
            Value v = evaluate(stmt->as.print.expr, it);
            if (!it->had_error) { value_print(v); printf("\n"); }
            break;
        }

        case STMT_EXPR: {
            evaluate(stmt->as.expr.expr, it);
            break;
        }

        case STMT_BLOCK: {
            /* Un blocco crea un NUOVO scope che racchiude quello esterno:
             * le variabili dichiarate qui dentro spariscono all'uscita. */
            Env block_env;
            env_init(&block_env);
            block_env.enclosing = it->env;
            Env *saved = it->env;
            it->env = &block_env;

            Program *body = &stmt->as.block.body;
            for (int i = 0; i < body->count; i++) {
                execute(body->statements[i], it);
                if (it->had_error || it->is_returning) break;
            }

            it->env = saved;
            env_free(&block_env);
            break;
        }

        case STMT_IF: {
            Value cond = evaluate(stmt->as.if_stmt.condition, it);
            if (it->had_error) break;
            if (is_truthy(cond)) {
                execute(stmt->as.if_stmt.then_branch, it);
            } else if (stmt->as.if_stmt.else_branch != NULL) {
                execute(stmt->as.if_stmt.else_branch, it);
            }
            break;
        }

        case STMT_WHILE: {
            for (;;) {
                Value cond = evaluate(stmt->as.while_stmt.condition, it);
                if (it->had_error) break;
                if (!is_truthy(cond)) break;
                execute(stmt->as.while_stmt.body, it);
                if (it->had_error || it->is_returning) break;
            }
            break;
        }

        case STMT_FUN: {
            /* Definisce una variabile col nome della funzione, il cui valore
             * e' la funzione stessa (un puntatore al suo nodo AST). */
            env_define(it->env, stmt->as.function.name, value_function(stmt));
            break;
        }

        case STMT_RETURN: {
            Value v;
            if (stmt->as.ret.value != NULL) v = evaluate(stmt->as.ret.value, it);
            else                            v = value_number(0);   /* return "vuoto" */
            if (it->had_error) break;
            it->return_value = v;
            it->is_returning = 1;   /* segnala: stiamo uscendo dalla funzione */
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
    it.globals = env;
    it.had_error = 0;
    it.is_returning = 0;
    it.return_value = value_number(0);
    it.strings = NULL;
    it.str_count = 0;
    it.str_cap = 0;

    for (int i = 0; i < program->count; i++) {
        execute(program->statements[i], &it);
        if (it.had_error || it.is_returning) break;
    }

    /* Libera le stringhe create a runtime (le concatenazioni). */
    for (int i = 0; i < it.str_count; i++) free(it.strings[i]);
    free(it.strings);

    *had_error = it.had_error;
}
