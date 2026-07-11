#ifndef PROV_H
#define PROV_H

#include <string.h>
#include "gc.h"

/*
 * PROVENIENZA dei valori: la memoria storica di 'trace' / 'why'.
 *
 * Quando una variabile e' tracciata ('trace x;'), ogni assegnazione le
 * registra un NODO DI PROVENIENZA: quale espressione l'ha prodotta, a che
 * riga, e da quali altre variabili (con il loro valore DI ALLORA). I nodi si
 * collegano tra loro e formano l'albero causale che 'why x;' stampa.
 *
 * Tre scelte di design da conoscere:
 *
 * 1. FOTOGRAFIE, NON RIFERIMENTI. I valori delle dipendenze sono salvati gia'
 *    resi in TESTO (val_text). Un puntatore mentirebbe: se dopo fai push su
 *    un array, la storia mostrerebbe il presente, non il passato. Il testo e'
 *    una fotografia immutabile. (Bonus: il GC non deve inseguire quei valori.)
 *
 * 2. OGGETTI DEL GC. Un nodo vive finche' qualcuno lo raggiunge (la Entry di
 *    una variabile, o il link di un altro nodo) e muore da solo quando non
 *    serve piu': stesso ciclo di vita di array e stringhe (tipo OBJ_PROV).
 *
 * 3. TETTO DI PROFONDITA'. Un contatore in un ciclo da un milione di giri
 *    creerebbe un milione di nodi concatenati. Oltre PROV_MAX_DEPTH livelli il
 *    nuovo nodo NON si collega alla storia vecchia (link = NULL, truncated=1):
 *    la catena resta corta e i nodi scollegati diventano spazzatura che il GC
 *    raccoglie. La storia e' una spiegazione recente, non un log infinito.
 */

#define PROV_MAX_DEPTH 20   /* massimi livelli di storia per catena          */
#define PROV_MAX_DEPS   8   /* massime variabili registrate per assegnazione */

typedef struct Prov Prov;

/* Una DIPENDENZA: una variabile letta dall'espressione assegnata. */
typedef struct {
    char *name;       /* il suo nome                                          */
    char *val_text;   /* il suo valore al momento (fotografia in testo)       */
    int   line;       /* riga della sua ultima assegnazione (0 = ignota)      */
    Prov *link;       /* la sua storia, se tracciata (NULL se no o troncata)  */
    int   truncated;  /* 1 = la storia c'era ma il tetto l'ha tagliata        */
} ProvDep;

struct Prov {
    Obj   obj;        /* intestazione GC: DEVE essere il primo campo          */
    int   line;       /* riga dell'assegnazione                               */
    char *target;     /* nome della variabile assegnata                       */
    char *expr_text;  /* l'espressione, ricostruita dall'AST                  */
    char *val_text;   /* il valore risultante, in testo                       */
    ProvDep *deps;    /* le variabili lette dall'espressione                  */
    int   dep_count;
    int   depth;      /* 1 + max profondita' dei link tenuti (<= PROV_MAX_DEPTH) */
};

/* Byte occupati dai buffer ESTERNI di un nodo (stringhe + array deps).
 * static inline nell'header: cosi' gc.c la usa per la contabilita' della
 * soglia senza dover linkare prov.c (gli strumenti di debug linkano gc.c
 * ma non l'evaluator). */
static inline size_t prov_extra_bytes(const Prov *p) {
    size_t n = 0;
    if (p->target    != NULL) n += strlen(p->target) + 1;
    if (p->expr_text != NULL) n += strlen(p->expr_text) + 1;
    if (p->val_text  != NULL) n += strlen(p->val_text) + 1;
    n += (size_t)p->dep_count * sizeof(ProvDep);
    for (int i = 0; i < p->dep_count; i++) {
        if (p->deps[i].name     != NULL) n += strlen(p->deps[i].name) + 1;
        if (p->deps[i].val_text != NULL) n += strlen(p->deps[i].val_text) + 1;
    }
    return n;
}

/* Stampa l'albero causale a partire da un nodo, con `indent` livelli di
 * rientro (2 spazi l'uno). Implementata in prov.c, usata da 'why'. */
void prov_print(const Prov *p, int indent);

#endif /* PROV_H */
