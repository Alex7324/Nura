#ifndef ENV_H
#define ENV_H

#include "value.h"

/*
 * L'ambiente (environment): la "memoria" delle variabili del programma.
 * E' una TABELLA HASH che associa un nome (stringa) a un VALORE (Value:
 * numero, booleano o stringa).
 *
 * Struttura: un array di bucket; una funzione di hash manda ogni nome in un
 * bucket; ogni bucket e' una lista concatenata (chaining) per gestire le
 * collisioni.
 */

#define ENV_CAPACITY 64   /* numero di bucket */

typedef struct Entry {
    char *name;           /* copia del nome (posseduta dalla tabella)      */
    Value value;          /* il valore associato                           */
    struct Entry *next;   /* prossima voce nello stesso bucket             */
} Entry;

typedef struct Env {
    Obj obj;                 /* intestazione GC: DEVE essere il primo campo    */
    Entry *buckets[ENV_CAPACITY];
    int count;
    struct Env *enclosing;   /* lo scope che racchiude questo (NULL = globale) */
} Env;

void env_init(Env *env);

/* Definisce una variabile (var). Se esiste gia', ne aggiorna il valore. */
void env_define(Env *env, const char *name, Value value);

/* Legge una variabile. Ritorna 1 e scrive in *out se esiste, 0 altrimenti. */
int env_get(Env *env, const char *name, Value *out);

/* Assegna a una variabile GIA' esistente. Ritorna 1 se c'era, 0 altrimenti. */
int env_assign(Env *env, const char *name, Value value);

/* Libera la memoria della tabella (nomi e voci). Le stringhe dei Value NON
 * sono liberate qui: sono "in prestito" (vivono nell'AST o nell'arena). */
void env_free(Env *env);

#endif /* ENV_H */
