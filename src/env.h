#ifndef ENV_H
#define ENV_H

/*
 * L'ambiente (environment): la "memoria" delle variabili del programma.
 * E' una TABELLA HASH che associa un nome (stringa) a un valore (double).
 *
 * Struttura:
 *   - un array di ENV_CAPACITY "bucket" (secchielli);
 *   - una funzione di hash manda ogni nome in un bucket (nome -> indice);
 *   - ogni bucket e' una LISTA CONCATENATA di voci (Entry), cosi' se due
 *     nomi diversi finiscono nello stesso bucket (collisione) convivono
 *     nella stessa lista. Questa tecnica si chiama "chaining".
 */

#define ENV_CAPACITY 64   /* numero di bucket (potenza di 2, comodo) */

/* Una voce della tabella: un nodo di lista concatenata. */
typedef struct Entry {
    char *name;           /* copia del nome (la tabella la possiede) */
    double value;         /* il valore associato                    */
    struct Entry *next;   /* prossima voce nello stesso bucket       */
} Entry;

typedef struct {
    Entry *buckets[ENV_CAPACITY];  /* array di teste di lista */
    int count;                     /* quante variabili in totale (per statistica) */
} Env;

/* Prepara una tabella vuota. */
void env_init(Env *env);

/* Definisce una variabile (var n = ...). Se esiste gia', ne aggiorna il valore. */
void env_define(Env *env, const char *name, double value);

/* Legge una variabile. Ritorna 1 e scrive in *out se esiste, 0 se non esiste. */
int env_get(Env *env, const char *name, double *out);

/* Assegna a una variabile GIA' esistente. Ritorna 1 se c'era, 0 altrimenti. */
int env_assign(Env *env, const char *name, double value);

/* Libera tutta la memoria della tabella (tutte le liste). */
void env_free(Env *env);

#endif /* ENV_H */
