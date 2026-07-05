#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>    /* floor, isfinite, fabs: per stampare bene i numeri */

Value value_number(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value value_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    if (b) v.as.boolean = 1;
    else   v.as.boolean = 0;
    return v;
}

Value value_string(char *s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = s;
    return v;
}

Value value_function(struct Stmt *decl, struct Env *closure) {
    Value v;
    v.type = VAL_FUNCTION;
    v.as.function.decl = decl;
    v.as.function.closure = closure;
    return v;
}

Value value_array(Array *arr) {
    Value v;
    v.type = VAL_ARRAY;
    v.as.array = arr;
    return v;
}

/* ------------------------------------------------------------------ */
/*  Array dinamico                                                    */
/* ------------------------------------------------------------------ */

/* La creazione passa dal GC, che alloca l'oggetto (header incluso) e lo
 * registra nella sua lista. Da qui in poi la memoria dell'array e' del GC. */
Array *array_new(void) {
    return gc_new_array();
}

/* Aggiunge un elemento in coda. Quando `items` e' pieno, RADDOPPIA la
 * capacita': cosi', ammortizzato, ogni push costa O(1). Identica strategia
 * di program_add e dell'arena delle stringhe. */
void array_push(Array *arr, Value v) {
    if (arr->count == arr->capacity) {
        int new_cap;
        if (arr->capacity < 8) new_cap = 8;
        else                   new_cap = arr->capacity * 2;
        Value *grown = realloc(arr->items, sizeof(Value) * (size_t)new_cap);
        if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
        arr->items = grown;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = v;
}

/* Libera il buffer degli elementi e la struttura stessa. NON tocca i singoli
 * elementi: le eventuali stringhe al loro interno vivono altrove (AST o arena
 * delle stringhe) e vengono liberate da chi le possiede. */
void array_free(Array *arr) {
    free(arr->items);
    free(arr);
}

/* Copia profonda: per una stringa ne alloca una nuova copia; per numeri e
 * booleani non c'e' niente da copiare (si ritorna il valore com'e').
 * Per un ARRAY copiamo solo il puntatore (semantica per riferimento): due
 * variabili condividono lo stesso array, quindi modificarlo da una si vede
 * anche dall'altra. */
Value value_copy(Value v) {
    if (v.type != VAL_STRING) return v;
    size_t size = strlen(v.as.string) + 1;
    char *copy = malloc(size);
    if (copy == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(copy, v.as.string, size);
    return value_string(copy);
}

/* Libera la stringa posseduta da un Value (no-op per numeri e booleani).
 * Per gli array e' un no-op: sono condivisi e vivono nell'arena, che li
 * liberera' a fine programma. Liberarli qui darebbe puntatori penzolanti. */
void value_free(Value v) {
    if (v.type == VAL_STRING) free(v.as.string);
}

/* Regola di verita': false e il numero 0 sono "falsi", tutto il resto e'
 * "vero" (una stringa, anche vuota, e' vera). Serve a if, while, ! , && , ||. */
int value_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:     return v.as.boolean;
        case VAL_NUMBER:   return v.as.number != 0;
        case VAL_STRING:   return 1;
        case VAL_FUNCTION: return 1;
        case VAL_ARRAY:    return 1;   /* un array (anche vuoto) e' "vero" */
    }
    return 0;
}

/* Due valori sono uguali solo se hanno lo stesso tipo e lo stesso contenuto.
 * (Quindi 1 e true sono diversi: tipi diversi.) */
int values_equal(Value a, Value b) {
    if (a.type != b.type) return 0;
    switch (a.type) {
        case VAL_NUMBER:   return a.as.number == b.as.number;
        case VAL_BOOL:     return a.as.boolean == b.as.boolean;
        case VAL_STRING:   return strcmp(a.as.string, b.as.string) == 0;
        case VAL_FUNCTION: return a.as.function.decl == b.as.function.decl;
        /* Due array sono "uguali" solo se sono LO STESSO array (stesso
         * puntatore), coerente con la semantica per riferimento: proprio come
         * per le funzioni, confrontiamo l'identita', non il contenuto. */
        case VAL_ARRAY:    return a.as.array == b.as.array;
    }
    return 0;
}

/* Limite di profondita' per la stampa degli array annidati. Serve soprattutto
 * come guard-rail contro gli array CICLICI: dato che gli array sono per
 * riferimento, si puo' scrivere  a[0] = a  e creare un array che contiene se
 * stesso. Senza questo limite, stamparlo ricorrerebbe all'infinito fino a far
 * esplodere lo stack. Oltre il limite stampiamo "[...]" e ci fermiamo. */
#define MAX_PRINT_DEPTH 100

static void value_print_depth(Value v, int depth);

/* Stampa un valore COME ELEMENTO di un array: le stringhe vengono racchiuse
 * tra virgolette, cosi' [1, "ciao"] non si confonde con [1, ciao]. Per gli
 * altri tipi si comporta come value_print. */
static void print_element(Value v, int depth) {
    if (v.type == VAL_STRING) printf("\"%s\"", v.as.string);
    else                      value_print_depth(v, depth);
}

static void value_print_depth(Value v, int depth) {
    switch (v.type) {
        case VAL_NUMBER: {
            double n = v.as.number;
            /* Se e' un intero "vero" (entro ~2^53, dove i double sono ancora
             * esatti) lo stampiamo per esteso: 1000000, non "1e+06". Altrimenti
             * usiamo %g, che tiene puliti i decimali (3.14, 0.333333). */
            if (isfinite(n) && n == floor(n) && fabs(n) < 1e15)
                printf("%.0f", n);
            else
                printf("%g", n);
            break;
        }
        case VAL_BOOL:
            if (v.as.boolean) printf("true");
            else              printf("false");
            break;
        case VAL_STRING:
            printf("%s", v.as.string);
            break;
        case VAL_FUNCTION:
            printf("<funzione>");
            break;
        case VAL_ARRAY: {
            Array *arr = v.as.array;
            if (depth >= MAX_PRINT_DEPTH) {   /* troppo annidato (o ciclico) */
                printf("[...]");
                break;
            }
            printf("[");
            for (int i = 0; i < arr->count; i++) {
                if (i > 0) printf(", ");
                print_element(arr->items[i], depth + 1);   /* ricorsivo: array annidati */
            }
            printf("]");
            break;
        }
    }
}

void value_print(Value v) {
    value_print_depth(v, 0);
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case VAL_NUMBER:   return "numero";
        case VAL_BOOL:     return "booleano";
        case VAL_STRING:   return "stringa";
        case VAL_FUNCTION: return "funzione";
        case VAL_ARRAY:    return "array";
    }
    return "?";
}
