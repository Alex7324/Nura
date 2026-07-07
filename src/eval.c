#include "eval.h"
#include "gc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>   /* clock() per la nativa clock() */

/* Limite di sicurezza alla PROFONDITA' DI RICORSIONE di evaluate()/execute()
 * (che si chiamano a vicenda). Oltre questa soglia diamo un errore controllato
 * invece di rischiare uno stack overflow del C (crash secco).
 *
 * Perche' contiamo evaluate+execute e non solo le chiamate di funzione? Perche'
 * lo stack C cresce sia con la ricorsione delle funzioni SIA con la profondita'
 * dell'espressione dentro ogni livello. Una funzione con un corpo "pesante"
 * (es. return [f(n-1)[0]+t[0]]) usa piu' stack per livello di una leggera (es.
 * return k*f(k-1)): contare solo le chiamate non basterebbe. */
#define MAX_DEPTH 2000

/*
 * Contesto dell'esecuzione:
 *   - env: l'ambiente delle variabili
 *   - had_error: bandierina di errore a runtime
 */
/* Con il tag 'Interp' (non anonima) cosi' combacia con la forward-declaration
 * 'struct Interp' di value.h, usata dal tipo NativeFn. */
typedef struct Interp Interp;
struct Interp {
    Env *env;          /* scope corrente */
    Env *globals;      /* scope globale (i corpi delle funzioni lo racchiudono) */
    int had_error;
    int is_returning;  /* 1 mentre un 'return' sta "risalendo" fuori dalla funzione */
    int is_breaking;   /* 1 mentre un 'break' sta "risalendo" fino al ciclo       */
    int is_continuing; /* 1 mentre un 'continue' sta "risalendo" fino al ciclo    */
    Value return_value;
    int depth;         /* profondita' di ricorsione di evaluate()/execute()     */
    /* NB: stringhe, array e ambienti sono tutti oggetti gestiti dal garbage
     * collector (vedi gc.c): niente piu' arene manuali qui dentro. */
};

/* evaluate/execute si chiamano a vicenda; ognuna e' un WRAPPER col guard-rail
 * di profondita', che delega il lavoro vero a *_impl. Dichiarazioni anticipate. */
static Value evaluate(Expr *expr, Interp *it);
static Value evaluate_impl(Expr *expr, Interp *it);
static void  execute(Stmt *stmt, Interp *it);
static void  execute_impl(Stmt *stmt, Interp *it);

/* Puntatore all'interprete in esecuzione: serve al GC per trovare le radici.
 * Uno solo alla volta gira, quindi va bene una variabile a livello di modulo. */
static Interp *g_it = NULL;

/* Le RADICI del garbage collector: cio' che e' sicuramente vivo. Il GC gira solo
 * ai confini sicuri (tra un'istruzione e l'altra, con call_depth 0), dove non c'e'
 * nessun temporaneo vivo in una variabile C: bastano l'ambiente globale e quello
 * corrente. Da li' il GC segue i puntatori e trova tutto il resto (closures,
 * array annidati, scope racchiudenti). */
static void mark_roots(void) {
    if (g_it == NULL) return;
    gc_mark_env(g_it->globals);
    gc_mark_env(g_it->env);
    /* Le radici temporanee (oggetti a meta' calcolo) e gli ambienti dei
     * chiamanti sospesi li protegge chi li ha in mano, con gc_push_temp. */
}

/* L'oggetto gestito dal GC referenziato da un Value (NULL se non ne referenzia).
 * Serve per proteggere un Value come radice temporanea con gc_push_temp. */
static Obj *value_obj(Value v) {
    if (v.type == VAL_ARRAY)    return (Obj *)v.as.array;
    if (v.type == VAL_STRING)   return (Obj *)v.as.string;
    if (v.type == VAL_FUNCTION) return (Obj *)v.as.function.closure;
    return NULL;
}

static void runtime_error(Interp *it, const char *message) {
    if (it->had_error) return;
    it->had_error = 1;
    fprintf(stderr, "Errore a runtime: %s\n", message);
}

/* Valida un accesso arr[i]: controlla che arr_v sia un array, che idx_v sia
 * un numero e che l'indice sia dentro i limiti. Se tutto ok, scrive l'indice
 * intero in *out e ritorna 1; altrimenti segnala l'errore e ritorna 0. */
/* Verifica che idx_v sia un indice valido per un contenitore di `count` elementi
 * (array O stringa): un numero INTERO e finito, dentro [0, count). Se ok, scrive
 * l'indice in *out e ritorna 1; altrimenti segnala l'errore (usando `what`, es.
 * "l'array" / "la stringa") e ritorna 0. */
static int valid_index(Interp *it, Value idx_v, int count, const char *what, int *out) {
    char msg[160];
    if (idx_v.type != VAL_NUMBER) {
        snprintf(msg, sizeof(msg), "l'indice deve essere un numero, non un valore di tipo %s.",
                 value_type_name(idx_v.type));
        runtime_error(it, msg);
        return 0;
    }
    /* L'indice deve essere INTERO e finito. Rifiutiamo i decimali (a[1.9]), i NaN
     * e gli infiniti (floor/isfinite li intercettano), evitando anche il cast a
     * int di valori enormi (undefined behavior in C). */
    double d = idx_v.as.number;
    if (!isfinite(d) || d != floor(d)) {
        snprintf(msg, sizeof(msg), "l'indice deve essere un numero intero, non %g.", d);
        runtime_error(it, msg);
        return 0;
    }
    /* Limiti controllati sul double (non ancora convertito): dopo questo controllo
     * sappiamo che 0 <= d < count <= INT_MAX, quindi il cast a int e' sicuro. */
    if (d < 0 || d >= count) {
        const char *suffisso;
        if (count == 1) suffisso = "o";   /* 1 elemento */
        else            suffisso = "i";   /* N elementi */
        snprintf(msg, sizeof(msg),
                 "indice %.0f fuori dai limiti: %s ha %d element%s.",
                 d, what, count, suffisso);
        runtime_error(it, msg);
        return 0;
    }
    *out = (int)d;
    return 1;
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

/* Concatena due stringhe in una NUOVA ObjString gestita dal GC. */
static Value concat(Interp *it, const char *a, const char *b) {
    (void)it;   /* non serve piu' l'arena: ci pensa il GC */
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *buf = malloc(la + lb + 1);
    if (buf == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);   /* copia anche il '\0' di b */
    ObjString *s = gc_new_string(buf, (int)(la + lb));
    free(buf);                     /* gc_new_string ha fatto la sua copia */
    return value_string(s);
}

/* Regola di verita' condivisa (delega a value.c). */
static int is_truthy(Value v) { return value_is_truthy(v); }

/* ------------------------------------------------------------------ */
/*  Funzioni native (scritte in C, esposte a Nura)                    */
/* ------------------------------------------------------------------ */

/* len(x): la lunghezza di un array o di una stringa. */
static Value native_len(Interp *it, int argc, Value *args) {
    (void)argc;   /* l'arieta' (1) e' gia' controllata da chi chiama */
    Value v = args[0];
    if (v.type == VAL_ARRAY)  return value_number(v.as.array->count);
    if (v.type == VAL_STRING) return value_number(v.as.string->length);
    runtime_error(it, "len() vuole un array o una stringa.");
    return value_number(0);
}

/* push(arr, x): aggiunge x in coda all'array (condiviso: la modifica si vede da
 * tutte le variabili che puntano allo stesso array). Ritorna la nuova lunghezza. */
static Value native_push(Interp *it, int argc, Value *args) {
    (void)argc;
    if (args[0].type != VAL_ARRAY) {
        runtime_error(it, "push() vuole un array come primo argomento.");
        return value_number(0);
    }
    array_push(args[0].as.array, args[1]);
    return value_number(args[0].as.array->count);
}

/* pop(arr): toglie e restituisce l'ultimo elemento dell'array (che si accorcia). */
static Value native_pop(Interp *it, int argc, Value *args) {
    (void)argc;
    if (args[0].type != VAL_ARRAY) {
        runtime_error(it, "pop() vuole un array.");
        return value_number(0);
    }
    Array *a = args[0].as.array;
    if (a->count == 0) {
        runtime_error(it, "pop() su un array vuoto.");
        return value_number(0);
    }
    return a->items[--a->count];   /* accorcia; la capacita' non cambia */
}

/* ---- str(x): converte un valore nella sua rappresentazione testuale ---- */

#define STR_MAX_DEPTH 100   /* per array annidati o ciclici */

/* Appende `s` a un buffer che cresce da solo (semplice string builder). */
static void sb_append(char **buf, int *len, int *cap, const char *s) {
    int slen = (int)strlen(s);
    if (*len + slen + 1 > *cap) {
        int nc;
        if (*cap < 32) nc = 32;
        else           nc = *cap;
        while (nc < *len + slen + 1) nc *= 2;
        char *g = realloc(*buf, (size_t)nc);
        if (g == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
        *buf = g; *cap = nc;
    }
    memcpy(*buf + *len, s, (size_t)slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

/* Scrive nel buffer la rappresentazione di v (gemello di value_print, ma su
 * stringa). Le stringhe dentro un array vengono messe tra virgolette. */
static void stringify(Value v, char **buf, int *len, int *cap, int depth) {
    char tmp[64];
    switch (v.type) {
        case VAL_NUMBER: {
            double n = v.as.number;
            if (isfinite(n) && n == floor(n) && fabs(n) < 1e15) snprintf(tmp, sizeof(tmp), "%.0f", n);
            else                                                snprintf(tmp, sizeof(tmp), "%g", n);
            sb_append(buf, len, cap, tmp);
            break;
        }
        case VAL_BOOL:
            if (v.as.boolean) sb_append(buf, len, cap, "true");
            else              sb_append(buf, len, cap, "false");
            break;
        case VAL_STRING:   sb_append(buf, len, cap, v.as.string->chars); break;
        case VAL_FUNCTION: sb_append(buf, len, cap, "<funzione>"); break;
        case VAL_NATIVE:
            snprintf(tmp, sizeof(tmp), "<nativa %s>", v.as.native.name);
            sb_append(buf, len, cap, tmp);
            break;
        case VAL_ARRAY: {
            if (depth >= STR_MAX_DEPTH) { sb_append(buf, len, cap, "[...]"); break; }
            Array *a = v.as.array;
            sb_append(buf, len, cap, "[");
            for (int i = 0; i < a->count; i++) {
                if (i > 0) sb_append(buf, len, cap, ", ");
                if (a->items[i].type == VAL_STRING) {
                    sb_append(buf, len, cap, "\"");
                    sb_append(buf, len, cap, a->items[i].as.string->chars);
                    sb_append(buf, len, cap, "\"");
                } else {
                    stringify(a->items[i], buf, len, cap, depth + 1);
                }
            }
            sb_append(buf, len, cap, "]");
            break;
        }
    }
}

static Value native_str(Interp *it, int argc, Value *args) {
    (void)it; (void)argc;
    char *buf = NULL; int len = 0, cap = 0;
    stringify(args[0], &buf, &len, &cap, 0);
    const char *chars;
    if (buf != NULL) chars = buf;   /* buf e' NULL se stringify non ha scritto nulla */
    else             chars = "";
    ObjString *s = gc_new_string(chars, len);
    free(buf);
    return value_string(s);
}

/* num(s): converte una stringa in un numero. Errore se non e' un numero valido. */
static Value native_num(Interp *it, int argc, Value *args) {
    (void)argc;
    if (args[0].type != VAL_STRING) {
        runtime_error(it, "num() vuole una stringa.");
        return value_number(0);
    }
    const char *s = args[0].as.string->chars;
    char *end;
    double d = strtod(s, &end);
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (end == s || *end != '\0') {
        runtime_error(it, "num(): la stringa non e' un numero valido.");
        return value_number(0);
    }
    return value_number(d);
}

/* clock(): secondi (con decimali) di tempo CPU da inizio programma. Per misurare. */
static Value native_clock(Interp *it, int argc, Value *args) {
    (void)it; (void)argc; (void)args;
    return value_number((double)clock() / (double)CLOCKS_PER_SEC);
}

/* input(): legge una riga da tastiera e la ritorna come stringa (senza a-capo). */
static Value native_input(Interp *it, int argc, Value *args) {
    (void)it; (void)argc; (void)args;
    char line[4096];
    if (fgets(line, sizeof(line), stdin) == NULL) return value_string(gc_new_string("", 0));
    int n = (int)strlen(line);
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) n--;   /* toglie l'a-capo */
    return value_string(gc_new_string(line, n));
}

/* int(x): la parte intera di un numero, verso lo zero (int(2.9)=2, int(-2.9)=-2).
 * Comoda per gli indici di array, che devono essere interi: es. int((a+b)/2). */
static Value native_int(Interp *it, int argc, Value *args) {
    (void)argc;
    if (args[0].type != VAL_NUMBER) {
        runtime_error(it, "int() vuole un numero.");
        return value_number(0);
    }
    return value_number(trunc(args[0].as.number));
}

/* rand(): un numero PSEUDOcasuale in [0, 1). Per un intero in [0, n) si usa
 * int(rand() * n). E' "pseudo" perche' generato da una formula: sembra casuale
 * ma e' deterministico dato il seme (che qui cambia a ogni avvio). */
static Value native_rand(Interp *it, int argc, Value *args) {
    (void)it; (void)argc; (void)args;
    return value_number((double)rand() / ((double)RAND_MAX + 1.0));
}

/* Registra le native nell'ambiente globale, prima di eseguire il programma. */
static void define_natives(Env *globals) {
    /* Semino il generatore pseudocasuale una volta, mescolando orologio di
     * sistema e tempo CPU: cosi' rand() da' una sequenza diversa a ogni avvio. */
    srand((unsigned)time(NULL) ^ (unsigned)clock());

    env_define(globals, "len",   value_native("len",   native_len,   1));
    env_define(globals, "push",  value_native("push",  native_push,  2));
    env_define(globals, "pop",   value_native("pop",   native_pop,   1));
    env_define(globals, "str",   value_native("str",   native_str,   1));
    env_define(globals, "num",   value_native("num",   native_num,   1));
    env_define(globals, "int",   value_native("int",   native_int,   1));
    env_define(globals, "rand",  value_native("rand",  native_rand,  0));
    env_define(globals, "clock", value_native("clock", native_clock, 0));
    env_define(globals, "input", value_native("input", native_input, 0));
}

/* ------------------------------------------------------------------ */
/*  Valutazione di un'espressione -> Value                            */
/* ------------------------------------------------------------------ */

/* Wrapper col guard-rail di profondita': conta la ricorsione e la limita. */
static Value evaluate(Expr *expr, Interp *it) {
    if (it->depth >= MAX_DEPTH) {
        runtime_error(it, "profondita' massima superata (ricorsione o espressione troppo profonda).");
        return value_number(0);
    }
    it->depth++;
    Value v = evaluate_impl(expr, it);
    it->depth--;
    return v;
}

static Value evaluate_impl(Expr *expr, Interp *it) {
    switch (expr->type) {

        case EXPR_NUMBER: return value_number(expr->as.number.value);
        case EXPR_BOOL:   return value_bool(expr->as.boolean.value);
        case EXPR_STRING:   /* crea una ObjString (gestita dal GC) dal literal */
            return value_string(gc_new_string(expr->as.string.value,
                                              (int)strlen(expr->as.string.value)));

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
            /* proteggo 'l' (se e' un oggetto) mentre valuto il destro: quello
             * potrebbe chiamare una funzione e far scattare una raccolta. */
            gc_push_temp(value_obj(l));
            Value r = evaluate(expr->as.binary.right, it);
            gc_pop_temp(1);
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
                    return concat(it, l.as.string->chars, r.as.string->chars);
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

            /* Chiamata a una funzione NATIVA (scritta in C). */
            if (callee.type == VAL_NATIVE) {
                int argc = expr->as.call.arg_count;
                if (callee.as.native.arity >= 0 && argc != callee.as.native.arity) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "la funzione '%s' vuole %d argomenti, ne hai passati %d.",
                             callee.as.native.name, callee.as.native.arity, argc);
                    runtime_error(it, msg);
                    return value_number(0);
                }
                Value *args = NULL;
                if (argc > 0) {
                    args = malloc(sizeof(Value) * (size_t)argc);
                    if (args == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
                }
                int pushed = 0;
                for (int i = 0; i < argc; i++) {
                    args[i] = evaluate(expr->as.call.args[i], it);
                    if (it->had_error) { gc_pop_temp(pushed); free(args); return value_number(0); }
                    gc_push_temp(value_obj(args[i]));   /* proteggo gli arg gia' pronti */
                    pushed++;
                }
                Value result = callee.as.native.fn(it, argc, args);
                gc_pop_temp(pushed);
                free(args);
                return result;
            }

            if (callee.type != VAL_FUNCTION) {
                runtime_error(it, "si possono chiamare solo le funzioni.");
                return value_number(0);
            }

            Stmt *decl = callee.as.function.decl;
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

            /* Protezioni GC. Da qui in poi si valutano gli argomenti (che
             * possono chiamare funzioni e far scattare una raccolta) e si esegue
             * il corpo. Proteggo: (1) la closure catturata dalla funzione, (2) il
             * nuovo ambiente di chiamata mentre non e' ancora quello corrente, e
             * (3) l'ambiente del CHIAMANTE, che non e' nella catena del nuovo
             * scope (call_env racchiude la closure, non il chiamante). */
            gc_push_temp((Obj *)callee.as.function.closure);   /* (1) */

            /* Nuovo scope per la chiamata. Racchiude l'ambiente CATTURATO dalla
             * funzione (la closure): cosi' vede le variabili di dove e' definita. */
            Env *call_env = gc_new_env();
            call_env->enclosing = callee.as.function.closure;
            gc_push_temp((Obj *)call_env);                     /* (2) */

            for (int i = 0; i < paramc; i++) {
                Value arg = evaluate(expr->as.call.args[i], it);
                if (it->had_error) { gc_pop_temp(2); return value_number(0); }
                env_define(call_env, decl->as.function.params[i], arg);
            }

            /* Il guard-rail anti-crash e' nel wrapper evaluate/execute (conta la
             * profondita' totale di ricorsione): una ricorsione infinita o troppo
             * profonda da' un errore controllato invece di far esplodere lo stack. */
            Env *saved = it->env;
            gc_push_temp((Obj *)saved);                        /* (3) */
            it->env = call_env;
            execute(decl->as.function.body, it);
            it->env = saved;
            gc_pop_temp(3);   /* (3),(2),(1): saved, call_env, closure */

            Value result;
            if (it->is_returning) {
                result = it->return_value;
                it->is_returning = 0;
            } else {
                result = value_number(0);
            }
            return result;
        }

        case EXPR_ARRAY: {
            /* Creiamo un nuovo array sull'heap (registrato nell'arena) e ci
             * mettiamo dentro, uno per uno, i valori degli elementi. */
            Array *arr = array_new();   /* creato e tracciato dal GC */
            /* Proteggo l'array (a meta' costruzione) mentre valuto gli elementi:
             * uno di essi potrebbe chiamare una funzione e far scattare il GC. */
            gc_push_temp((Obj *)arr);
            for (int i = 0; i < expr->as.array.count; i++) {
                Value elem = evaluate(expr->as.array.elements[i], it);
                if (it->had_error) { gc_pop_temp(1); return value_number(0); }
                array_push(arr, elem);
            }
            gc_pop_temp(1);
            return value_array(arr);
        }

        case EXPR_INDEX: {
            Value arr_v = evaluate(expr->as.index.array, it);
            if (it->had_error) return arr_v;
            gc_push_temp(value_obj(arr_v));   /* proteggo arr_v mentre valuto l'indice */
            Value idx_v = evaluate(expr->as.index.index, it);
            gc_pop_temp(1);
            if (it->had_error) return idx_v;

            int index;
            /* Si possono indicizzare gli array E le stringhe. */
            if (arr_v.type == VAL_ARRAY) {
                if (!valid_index(it, idx_v, arr_v.as.array->count, "l'array", &index))
                    return value_number(0);
                return arr_v.as.array->items[index];
            }
            if (arr_v.type == VAL_STRING) {
                if (!valid_index(it, idx_v, arr_v.as.string->length, "la stringa", &index))
                    return value_number(0);
                /* Nura non ha un tipo "carattere": il carattere e' una stringa di 1. */
                return value_string(gc_new_string(&arr_v.as.string->chars[index], 1));
            }
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "si puo' indicizzare solo un array o una stringa, non un valore di tipo %s.",
                     value_type_name(arr_v.type));
            runtime_error(it, msg);
            return value_number(0);
        }

        case EXPR_INDEX_SET: {
            Value arr_v = evaluate(expr->as.index_set.array, it);
            if (it->had_error) return arr_v;
            gc_push_temp(value_obj(arr_v));   /* proteggo arr_v per indice e valore */
            Value idx_v = evaluate(expr->as.index_set.index, it);
            if (it->had_error) { gc_pop_temp(1); return idx_v; }
            Value val = evaluate(expr->as.index_set.value, it);
            gc_pop_temp(1);
            if (it->had_error) return val;

            /* La scrittura per indice vale solo per gli array: le stringhe sono
             * immutabili e condivise, non si modificano sul posto. */
            if (arr_v.type != VAL_ARRAY) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "si puo' assegnare per indice solo a un array, non a un valore di tipo %s.",
                         value_type_name(arr_v.type));
                runtime_error(it, msg);
                return value_number(0);
            }
            int index;
            if (!valid_index(it, idx_v, arr_v.as.array->count, "l'array", &index))
                return value_number(0);
            /* L'array e' condiviso: scrivere qui si vede da tutte le variabili
             * che puntano allo stesso array. */
            arr_v.as.array->items[index] = val;
            return val;   /* un assegnamento "vale" il valore assegnato */
        }
    }
    return value_number(0);
}

/* ------------------------------------------------------------------ */
/*  Esecuzione di un'istruzione                                       */
/* ------------------------------------------------------------------ */

/* Wrapper col guard-rail di profondita' (gemello di quello di evaluate). */
static void execute(Stmt *stmt, Interp *it) {
    if (it->depth >= MAX_DEPTH) {
        runtime_error(it, "profondita' massima superata (ricorsione o espressione troppo profonda).");
        return;
    }
    it->depth++;
    execute_impl(stmt, it);
    it->depth--;
}

static void execute_impl(Stmt *stmt, Interp *it) {
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
             * le variabili dichiarate qui dentro non escono dal blocco.
             * Heap e non liberato: una closure definita nel blocco potrebbe
             * catturarlo (stesso motivo delle chiamate). */
            Env *block_env = gc_new_env();
            block_env->enclosing = it->env;
            Env *saved = it->env;
            it->env = block_env;

            Program *body = &stmt->as.block.body;
            for (int i = 0; i < body->count; i++) {
                /* Confine sicuro tra un'istruzione e l'altra: qui non c'e' nessun
                 * temporaneo vivo "a meta' calcolo", quindi il GC puo' girare in
                 * sicurezza (anche dentro le chiamate: gli ambienti sospesi e i
                 * temporanei delle espressioni esterne sono protetti da gc_push_temp).
                 * E' qui che si recupera la spazzatura dei giri di un ciclo. */
                gc_maybe_collect();
                execute(body->statements[i], it);
                /* Se e' scattato return/break/continue, smetto di eseguire il
                 * blocco: il segnale "risale" fino a chi lo gestisce (la funzione
                 * per return, il ciclo per break/continue). */
                if (it->had_error || it->is_returning ||
                    it->is_breaking || it->is_continuing) break;
            }

            it->env = saved;
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
                /* Confine sicuro a ogni giro: copre anche i while col corpo di
                 * una sola istruzione (senza blocco), che non passerebbero dal
                 * punto di raccolta di STMT_BLOCK. */
                gc_maybe_collect();
                Value cond = evaluate(stmt->as.while_stmt.condition, it);
                if (it->had_error) break;
                if (!is_truthy(cond)) break;
                execute(stmt->as.while_stmt.body, it);
                if (it->had_error || it->is_returning) break;
                if (it->is_breaking)   { it->is_breaking = 0; break; }
                if (it->is_continuing) { it->is_continuing = 0; }  /* -> prossimo giro */
            }
            break;
        }

        case STMT_FOR: {
            /* Un nuovo scope per la variabile del for: cosi' non "esce" dal ciclo.
             * (Il corpo, se e' un blocco, crea comunque il SUO scope a ogni giro.) */
            Env *loop_env = gc_new_env();
            loop_env->enclosing = it->env;
            Env *saved = it->env;
            it->env = loop_env;

            if (stmt->as.for_stmt.initializer != NULL)
                execute(stmt->as.for_stmt.initializer, it);

            while (!it->had_error && !it->is_returning) {
                gc_maybe_collect();
                if (stmt->as.for_stmt.condition != NULL) {
                    Value cond = evaluate(stmt->as.for_stmt.condition, it);
                    if (it->had_error) break;
                    if (!is_truthy(cond)) break;
                }
                execute(stmt->as.for_stmt.body, it);
                if (it->had_error || it->is_returning) break;
                if (it->is_breaking) { it->is_breaking = 0; break; }
                if (it->is_continuing) it->is_continuing = 0;  /* NON esce: fa l'incremento */

                /* L'incremento gira SEMPRE a fine giro, anche dopo un continue:
                 * e' la differenza chiave rispetto alla vecchia traduzione in while. */
                if (stmt->as.for_stmt.increment != NULL) {
                    evaluate(stmt->as.for_stmt.increment, it);
                    if (it->had_error) break;
                }
            }

            it->env = saved;
            break;
        }

        case STMT_BREAK:    it->is_breaking = 1;    break;
        case STMT_CONTINUE: it->is_continuing = 1;  break;

        case STMT_FUN: {
            /* La funzione cattura l'ambiente corrente (it->env) come sua
             * "closure": cosi' ricordera' le variabili di dove e' definita. */
            env_define(it->env, stmt->as.function.name,
                       value_function(stmt, it->env));
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
    it.is_breaking = 0;
    it.is_continuing = 0;
    it.depth = 0;

    /* Rendo disponibili le funzioni native (len, push, ...) nell'ambiente
     * globale, prima di eseguire il programma. */
    define_natives(env);

    /* Dico al GC dove trovare le radici (via mark_roots, che legge g_it). */
    g_it = &it;
    gc_set_mark_roots(mark_roots);

    for (int i = 0; i < program->count; i++) {
        gc_maybe_collect();   /* confine sicuro tra le istruzioni top-level */
        execute(program->statements[i], &it);
        if (it.had_error || it.is_returning) break;
    }

    /* L'interprete 'it' e' locale: scollego il GC prima di uscire, cosi' non
     * resta un puntatore penzolante. gc_free_all (in main.c) non usa le radici. */
    gc_set_mark_roots(NULL);
    g_it = NULL;

    /* Niente da liberare a mano: stringhe, array e ambienti sono del garbage
     * collector, che li liberera' con gc_free_all (da main.c a fine programma). */

    *had_error = it.had_error;
}
