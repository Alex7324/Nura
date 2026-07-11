#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void env_init(Env *env) {
    for (int i = 0; i < ENV_CAPACITY; i++) {
        env->buckets[i] = NULL;   /* ogni bucket parte come lista vuota */
    }
    env->count = 0;
    env->enclosing = NULL;        /* lo scope racchiudente lo imposta il chiamante */
}

/*
 * La funzione di hash: djb2 (di Dan Bernstein), semplice e famosa.
 * Parte da 5381 e per ogni carattere fa  hash = hash * 33 + carattere.
 * Il "* 33" e' scritto come (hash << 5) + hash, che e' la stessa cosa
 * ma piu' veloce (hash*32 + hash*1). Il risultato e' un numero grande e
 * "ben mescolato"; sara' chi chiama a fare  % ENV_CAPACITY  per l'indice.
 */
static unsigned long hash_string(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++) != 0) {
        hash = ((hash << 5) + hash) + c;   /* hash * 33 + c */
    }
    return hash;
}

/* Alloca una copia di una stringa (la tabella deve possedere i nomi,
 * non puo' fidarsi di puntatori altrui che potrebbero sparire). */
static char *copy_string(const char *s) {
    size_t size = strlen(s) + 1;   /* +1 per il terminatore '\0' */
    char *copy = malloc(size);
    if (copy == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(copy, s, size);
    return copy;
}

/* Cerca la voce di nome `name`. Ritorna il puntatore alla Entry, o NULL.
 * E' il cuore della ricerca O(1): calcola il bucket e scorre solo QUELLA
 * lista, non tutta la tabella. */
static Entry *find_entry(Env *env, const char *name) {
    unsigned long index = hash_string(name) % ENV_CAPACITY;
    for (Entry *e = env->buckets[index]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return e;   /* trovata */
        }
    }
    return NULL;        /* non c'e' in questo bucket => non esiste */
}

void env_define(Env *env, const char *name, Value value) {
    Entry *existing = find_entry(env, name);
    if (existing != NULL) {
        value_free(existing->value);          /* libera il vecchio valore */
        existing->value = value_copy(value);  /* l'ambiente ne possiede una copia */
        return;
    }

    /* Nuova variabile: creo una Entry e la inserisco IN TESTA alla lista
     * del suo bucket (inserimento in testa = O(1), non serve scorrere). */
    unsigned long index = hash_string(name) % ENV_CAPACITY;
    Entry *e = malloc(sizeof(Entry));
    if (e == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    e->name = copy_string(name);
    e->value = value_copy(value);   /* copia posseduta dall'ambiente */
    e->tracked = 0;                 /* provenienza (trace/why): spenta       */
    e->last_line = 0;
    e->prov = NULL;
    e->next = env->buckets[index];  /* il vecchio primo diventa il secondo */
    env->buckets[index] = e;        /* la nuova voce diventa la testa      */
    env->count++;
}

int env_get(Env *env, const char *name, Value *out) {
    Entry *e = find_entry(env, name);
    if (e != NULL) { *out = e->value; return 1; }
    /* non trovata qui: cerchiamo nello scope che ci racchiude */
    if (env->enclosing != NULL) return env_get(env->enclosing, name, out);
    return 0;
}

Entry *env_find(Env *env, const char *name) {
    Entry *e = find_entry(env, name);
    if (e != NULL) return e;
    if (env->enclosing != NULL) return env_find(env->enclosing, name);
    return NULL;
}

int env_assign(Env *env, const char *name, Value value) {
    Entry *e = find_entry(env, name);
    if (e != NULL) {
        value_free(e->value);
        e->value = value_copy(value);
        return 1;
    }
    /* non e' qui: proviamo ad assegnare nello scope racchiudente */
    if (env->enclosing != NULL) return env_assign(env->enclosing, name, value);
    return 0;   /* non esiste in nessuno scope */
}

void env_free(Env *env) {
    for (int i = 0; i < ENV_CAPACITY; i++) {
        Entry *e = env->buckets[i];
        while (e != NULL) {           /* libero l'intera lista del bucket */
            Entry *next = e->next;    /* salvo il prossimo PRIMA di liberare */
            value_free(e->value);     /* la stringa posseduta, se c'e'      */
            free(e->name);            /* la copia del nome                 */
            free(e);                  /* la voce                           */
            e = next;
        }
        env->buckets[i] = NULL;
    }
    env->count = 0;
}
