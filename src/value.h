#ifndef VALUE_H
#define VALUE_H

/*
 * Un Value e' un valore del linguaggio a runtime.
 *
 * Finora ogni valore era un semplice double. Ora un valore puo' essere:
 *   - un NUMERO   (double)
 *   - un BOOLEANO (true / false)
 *   - una STRINGA (testo)
 *
 * E' una "tagged union", lo stesso schema dei nodi dell'AST: un'etichetta
 * `type` che dice di che tipo e', piu' i dati veri e propri nella union.
 *
 * Nota sulla memoria: il campo `string` NON e' posseduto dal Value. Punta
 * a una stringa che vive altrove e piu' a lungo: o nell'AST (i letterali
 * "..."), o nell'"arena" dell'interprete (le stringhe create a runtime,
 * es. dalle concatenazioni). Cosi' i Value si possono copiare liberamente
 * senza preoccuparsi di chi libera cosa.
 */

struct Stmt;   /* nodo AST della funzione (corpo + parametri) */
struct Env;    /* ambiente catturato dalla closure               */

typedef enum {
    VAL_NUMBER,
    VAL_BOOL,
    VAL_STRING,
    VAL_FUNCTION
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        int boolean;    /* 0 = false, 1 = true */
        char *string;   /* puntatore "in prestito" (non posseduto) */
        /* Una funzione porta con se' due cose: il suo corpo (decl, nell'AST)
         * e l'ambiente dove e' stata DEFINITA (closure). E' la closure che le
         * permette di "ricordare" le variabili di quel posto. */
        struct { struct Stmt *decl; struct Env *closure; } function;
    } as;
} Value;

/* Costruttori */
Value value_number(double n);
Value value_bool(int b);
Value value_string(char *s);
Value value_function(struct Stmt *decl, struct Env *closure);

/* Gestione memoria delle stringhe.
 * value_copy: copia profonda (duplica la stringa) — usata dall'ambiente, che
 *   deve possedere una sua copia perche' sopravvive ai singoli comandi.
 * value_free: libera la stringa (no-op per numeri e booleani). */
Value value_copy(Value v);
void  value_free(Value v);

/* Utilita' */
int  value_is_truthy(Value v);        /* false e 0 sono falsi, il resto e' vero */
int  values_equal(Value a, Value b);  /* uguaglianza (usata da == e !=)         */
void value_print(Value v);            /* stampa il valore, senza a-capo         */
const char *value_type_name(ValueType t);  /* "numero"/"booleano"/"stringa"     */

#endif /* VALUE_H */
