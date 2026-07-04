#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Copia profonda: per una stringa ne alloca una nuova copia; per numeri e
 * booleani non c'e' niente da copiare (si ritorna il valore com'e'). */
Value value_copy(Value v) {
    if (v.type != VAL_STRING) return v;
    size_t size = strlen(v.as.string) + 1;
    char *copy = malloc(size);
    if (copy == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(copy, v.as.string, size);
    return value_string(copy);
}

/* Libera la stringa posseduta da un Value (no-op per numeri e booleani). */
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
    }
    return 0;
}

void value_print(Value v) {
    switch (v.type) {
        case VAL_NUMBER:
            printf("%g", v.as.number);
            break;
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
    }
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case VAL_NUMBER:   return "numero";
        case VAL_BOOL:     return "booleano";
        case VAL_STRING:   return "stringa";
        case VAL_FUNCTION: return "funzione";
    }
    return "?";
}
