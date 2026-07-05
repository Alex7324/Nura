#ifndef VALUE_H
#define VALUE_H

#include "gc.h"   /* per Obj: l'intestazione GC di Array */

/*
 * Un Value e' un valore del linguaggio a runtime.
 *
 * Finora ogni valore era un semplice double. Ora un valore puo' essere:
 *   - un NUMERO   (double)
 *   - un BOOLEANO (true / false)
 *   - una STRINGA (testo)
 *   - una FUNZIONE
 *   - un ARRAY    (lista dinamica di altri Value)
 *
 * E' una "tagged union", lo stesso schema dei nodi dell'AST: un'etichetta
 * `type` che dice di che tipo e', piu' i dati veri e propri nella union.
 *
 * Nota sulla memoria: il campo `string` NON e' posseduto dal Value. Punta
 * a una stringa che vive altrove e piu' a lungo: o nell'AST (i letterali
 * "..."), o nell'"arena" dell'interprete (le stringhe create a runtime,
 * es. dalle concatenazioni). Cosi' i Value si possono copiare liberamente
 * senza preoccuparsi di chi libera cosa.
 *
 * Anche l'ARRAY e' "in prestito": il campo `array` e' un puntatore a una
 * struttura Array che vive sull'heap. Copiare un Value-array copia solo il
 * PUNTATORE, non l'array: due variabili possono quindi condividere lo stesso
 * array (semantica "per riferimento", come in Python/JavaScript). Chi lo
 * libera? Nessuno durante l'esecuzione: gli array vivono fino a fine
 * programma (registrati in un'arena, come le stringhe). E' la stessa scelta
 * fatta per gli ambienti delle closure, ed e' cio' che un domani chiudera'
 * il garbage collector.
 */

struct Stmt;   /* nodo AST della funzione (corpo + parametri) */
struct Env;    /* ambiente catturato dalla closure               */

typedef enum {
    VAL_NUMBER,
    VAL_BOOL,
    VAL_STRING,
    VAL_FUNCTION,
    VAL_ARRAY
} ValueType;

/* Dichiarazioni anticipate: Value e Array si riferiscono a vicenda (un Value
 * puo' essere un Array, e un Array contiene dei Value), quindi nessuno dei due
 * puo' essere definito "prima" dell'altro. I nomi bastano per i puntatori. */
typedef struct Value Value;
typedef struct Array Array;

/* Un Array e' un array dinamico di Value: stessa idea di Program (lista di
 * Stmt*) o della tabella hash. `items` cresce raddoppiando quando si riempie. */
struct Array {
    Obj obj;        /* intestazione GC: DEVE essere il primo campo */
    Value *items;
    int count;      /* quanti elementi ci sono davvero          */
    int capacity;   /* quanti ce ne stanno prima di dover crescere */
};

struct Value {
    ValueType type;
    union {
        double number;
        int boolean;    /* 0 = false, 1 = true */
        char *string;   /* puntatore "in prestito" (non posseduto) */
        /* Una funzione porta con se' due cose: il suo corpo (decl, nell'AST)
         * e l'ambiente dove e' stata DEFINITA (closure). E' la closure che le
         * permette di "ricordare" le variabili di quel posto. */
        struct { struct Stmt *decl; struct Env *closure; } function;
        Array *array;   /* puntatore condiviso, non posseduto dal Value */
    } as;
};

/* Costruttori */
Value value_number(double n);
Value value_bool(int b);
Value value_string(char *s);
Value value_function(struct Stmt *decl, struct Env *closure);
Value value_array(Array *arr);

/* Gestione degli array (l'arena in eval.c possiede la memoria). */
Array *array_new(void);              /* alloca un array vuoto sull'heap */
void   array_push(Array *arr, Value v);   /* aggiunge in coda, cresce se serve */
void   array_free(Array *arr);       /* libera items + la struttura Array */

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
