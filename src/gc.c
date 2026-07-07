#include "gc.h"
#include "value.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Garbage collector mark-and-sweep.
 *
 *  MARK  : dalle "radici" (gli ambienti vivi + il valore di return in transito)
 *          marchiamo, seguendo i puntatori, TUTTO cio' che e' raggiungibile.
 *          I cicli (es. un array che contiene se stesso) non sono un problema:
 *          un oggetto gia' marcato non viene rivisitato.
 *  SWEEP : scorriamo la lista di TUTTI gli oggetti e liberiamo quelli non
 *          marcati (irraggiungibili = spazzatura).
 *
 * QUANDO gira: solo ai confini tra istruzioni al livello 0 (vedi eval.c). Li'
 * non c'e' nessun temporaneo vivo in una variabile C, quindi le radici sono
 * solo gli ambienti: semplice e sicuro.
 *
 * COME marchiamo: NON per ricorsione (un albero di oggetti profondo la farebbe
 * esplodere), ma con una "gray stack": si marca un oggetto e lo si mette in
 * lista; poi, in un ciclo, se ne estraggono e si marcano i figli. Iterativo.
 */

static Obj   *g_objects = NULL;   /* lista di tutti gli oggetti gestiti     */
static size_t g_bytes    = 0;     /* stima dei byte vivi (per la soglia)    */
static size_t g_count    = 0;     /* quanti oggetti vivi (per il log)       */
static size_t g_next_gc  = (size_t)1 << 20;   /* soglia: prima raccolta a ~1 MB */
static void (*g_mark_roots)(void) = NULL;     /* callback che marca le radici   */

/* Interruttori di diagnostica, letti dall'ambiente in gc_init():
 *   NURA_GC_LOG=1     -> stampa una riga a ogni raccolta
 *   NURA_GC_STRESS=1  -> raccoglie a OGNI confine sicuro (per stanare bug) */
static int g_log = 0;
static int g_stress = 0;

/* La "gray stack": oggetti marcati ma di cui non abbiamo ancora marcato i figli. */
static Obj **g_gray = NULL;
static int   g_gray_count = 0;
static int   g_gray_cap = 0;

/* La pila delle RADICI TEMPORANEE: oggetti vivi solo in variabili C durante il
 * calcolo di un'espressione (es. un array a meta' costruzione, l'operando
 * sinistro di un + mentre valuto il destro). eval.c li protegge con push/pop
 * cosi' una raccolta scattata dentro una chiamata non li spazza via. */
static Obj **g_temp = NULL;
static int   g_temp_count = 0;
static int   g_temp_cap = 0;

void gc_push_temp(Obj *o) {
    if (g_temp_count == g_temp_cap) {
        int nc;
        if (g_temp_cap < 8) nc = 8;
        else                nc = g_temp_cap * 2;
        Obj **grown = realloc(g_temp, sizeof(Obj *) * (size_t)nc);
        if (grown == NULL) { fprintf(stderr, "Memoria esaurita (GC).\n"); exit(1); }
        g_temp = grown;
        g_temp_cap = nc;
    }
    g_temp[g_temp_count++] = o;   /* o puo' essere NULL: verra' ignorato in mark */
}

void gc_pop_temp(int n) {
    g_temp_count -= n;
}

/* Byte "occupati" da un oggetto, per la contabilita' della soglia (stima: la
 * sola struttura, ignoriamo items/voci; basta a decidere QUANDO raccogliere). */
/* Byte "occupati" da un oggetto: la struttura PIU' i suoi buffer esterni, cosi'
 * la soglia riflette la memoria vera. Deve restare simmetrico con quanto viene
 * aggiunto a g_bytes durante la vita dell'oggetto (alloc + crescite). */
static size_t obj_size(Obj *o) {
    switch (o->type) {
        case OBJ_ARRAY:
            return sizeof(Array) + (size_t)((Array *)o)->capacity * sizeof(Value);
        case OBJ_ENV:
            return sizeof(Env);   /* i bucket sono inline nella struct */
        case OBJ_STRING:
            return sizeof(ObjString) + (size_t)((ObjString *)o)->length + 1;
    }
    return 0;
}

void gc_init(void) {
    g_objects = NULL;
    g_bytes = 0;
    g_count = 0;
    g_next_gc = (size_t)1 << 20;
    g_gray_count = 0;
    g_temp_count = 0;
    g_log = (getenv("NURA_GC_LOG") != NULL);
    g_stress = (getenv("NURA_GC_STRESS") != NULL);
}

void gc_set_mark_roots(void (*fn)(void)) {
    g_mark_roots = fn;
}

/* ------------------------------------------------------------------ */
/*  Allocazione                                                       */
/* ------------------------------------------------------------------ */

static void gc_collect(void);

static void *alloc_object(size_t size, ObjType type) {
    /* Se siamo oltre la soglia e sappiamo trovare le radici, raccogliamo PRIMA
     * di allocare (il nuovo oggetto ancora non esiste, quindi non rischia di
     * essere spazzato). La chiamata effettiva e' filtrata da eval.c ai confini
     * sicuri: qui la soglia decide solo se "vale la pena". */
    Obj *o = malloc(size);
    if (o == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    o->type = type;
    o->marked = 0;
    o->next = g_objects;
    g_objects = o;
    g_bytes += size;
    g_count++;
    return o;
}

struct Array *gc_new_array(void) {
    Array *a = alloc_object(sizeof(Array), OBJ_ARRAY);
    a->items = NULL;
    a->count = 0;
    a->capacity = 0;
    return a;
}

struct Env *gc_new_env(void) {
    Env *e = alloc_object(sizeof(Env), OBJ_ENV);
    env_init(e);
    return e;
}

struct ObjString *gc_new_string(const char *chars, int length) {
    ObjString *s = alloc_object(sizeof(ObjString), OBJ_STRING);
    s->chars = malloc((size_t)length + 1);
    if (s->chars == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(s->chars, chars, (size_t)length);
    s->chars[length] = '\0';
    s->length = length;
    g_bytes += (size_t)length + 1;   /* conta anche il buffer dei caratteri */
    return s;
}

/* Comunica al GC che sono stati allocati altri `n` byte per un buffer esterno di
 * un oggetto (usata da array_push quando l'array cresce). */
void gc_count_bytes(size_t n) {
    g_bytes += n;
}

/* ------------------------------------------------------------------ */
/*  MARK                                                              */
/* ------------------------------------------------------------------ */

/* Marca un oggetto e lo mette nella gray stack (se non era gia' marcato). */
void gc_mark_object(Obj *o) {
    if (o == NULL || o->marked) return;
    o->marked = 1;

    if (g_gray_count == g_gray_cap) {
        int new_cap;
        if (g_gray_cap < 8) new_cap = 8;
        else                new_cap = g_gray_cap * 2;
        Obj **grown = realloc(g_gray, sizeof(Obj *) * (size_t)new_cap);
        if (grown == NULL) { fprintf(stderr, "Memoria esaurita (GC).\n"); exit(1); }
        g_gray = grown;
        g_gray_cap = new_cap;
    }
    g_gray[g_gray_count++] = o;
}

void gc_mark_env(struct Env *env) {
    gc_mark_object((Obj *)env);   /* Obj e' il primo campo: cast valido */
}

/* Marca gli oggetti eventualmente referenziati da un Value. */
static void mark_value(Value v) {
    if (v.type == VAL_ARRAY)         gc_mark_object((Obj *)v.as.array);
    else if (v.type == VAL_FUNCTION) gc_mark_object((Obj *)v.as.function.closure);
    else if (v.type == VAL_STRING)   gc_mark_object((Obj *)v.as.string);
    /* numeri e booleani non referenziano oggetti gestiti */
}

/* "Annerisce" un oggetto grigio: marca tutto cio' che referenzia. */
static void blacken(Obj *o) {
    switch (o->type) {
        case OBJ_ARRAY: {
            Array *a = (Array *)o;
            for (int i = 0; i < a->count; i++) mark_value(a->items[i]);
            break;
        }
        case OBJ_ENV: {
            Env *e = (Env *)o;
            for (int i = 0; i < ENV_CAPACITY; i++)
                for (Entry *en = e->buckets[i]; en != NULL; en = en->next)
                    mark_value(en->value);
            gc_mark_env(e->enclosing);   /* anche lo scope che lo racchiude */
            break;
        }
        case OBJ_STRING:
            break;   /* una stringa non referenzia altri oggetti: e' una foglia */
    }
}

/* ------------------------------------------------------------------ */
/*  SWEEP                                                             */
/* ------------------------------------------------------------------ */

static void free_object(Obj *o) {
    g_bytes -= obj_size(o);
    g_count--;
    switch (o->type) {
        case OBJ_ARRAY: array_free((Array *)o);        break;
        case OBJ_ENV:   env_free((Env *)o); free(o);   break;
        case OBJ_STRING: {
            ObjString *s = (ObjString *)o;
            free(s->chars);
            free(s);
            break;
        }
    }
}

static void sweep(void) {
    Obj **link = &g_objects;
    while (*link != NULL) {
        Obj *o = *link;
        if (o->marked) {
            o->marked = 0;        /* pulisco per la prossima raccolta */
            link = &o->next;
        } else {
            *link = o->next;      /* scollego dalla lista */
            free_object(o);       /* e libero             */
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Il ciclo di raccolta                                             */
/* ------------------------------------------------------------------ */

static void gc_collect(void) {
    size_t before_count = g_count;
    size_t before_bytes = g_bytes;

    /* 1. MARK: parti dalle radici (ambienti vivi + radici temporanee), poi
     * svuota la gray stack annerendo i figli. */
    g_gray_count = 0;
    if (g_mark_roots) g_mark_roots();
    for (int i = 0; i < g_temp_count; i++) gc_mark_object(g_temp[i]);
    while (g_gray_count > 0) {
        Obj *o = g_gray[--g_gray_count];
        blacken(o);
    }

    /* 2. SWEEP: libera tutto cio' che non e' stato marcato. */
    sweep();

    /* 3. Aggiorna la soglia: la prossima raccolta quando la memoria viva
     * raddoppia (con un minimo). Cosi' il GC si auto-regola. */
    g_next_gc = g_bytes * 2;
    if (g_next_gc < ((size_t)1 << 20)) g_next_gc = (size_t)1 << 20;

    if (g_log) {
        fprintf(stderr,
            "[GC] liberati %zu oggetti (%zu -> %zu vivi) | %zu -> %zu byte | prossima soglia %zu\n",
            before_count - g_count, before_count, g_count,
            before_bytes, g_bytes, g_next_gc);
    }
}

/* Chiamata da eval.c ai confini sicuri: raccoglie se siamo oltre soglia (o
 * sempre, in modalita' stress) e sappiamo trovare le radici. */
void gc_maybe_collect(void) {
    if (g_mark_roots != NULL && (g_stress || g_bytes > g_next_gc)) {
        gc_collect();
    }
}

/* ------------------------------------------------------------------ */
/*  Chiusura                                                         */
/* ------------------------------------------------------------------ */

void gc_free_all(void) {
    Obj *o = g_objects;
    while (o != NULL) {
        Obj *next = o->next;
        free_object(o);
        o = next;
    }
    g_objects = NULL;
    free(g_gray);
    g_gray = NULL;
    g_gray_count = 0;
    g_gray_cap = 0;
    free(g_temp);
    g_temp = NULL;
    g_temp_count = 0;
    g_temp_cap = 0;
}
