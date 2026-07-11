#ifndef GC_H
#define GC_H

#include <stddef.h>

/*
 * Il garbage collector di Nura (mark-and-sweep).
 *
 * Ogni oggetto vivo sull'heap gestito dal GC (per ora Array ed Env) porta in
 * testa una intestazione comune, Obj. Tutti gli oggetti sono agganciati a
 * un'unica lista globale, cosi' la fase di "sweep" puo' scorrerli tutti.
 *
 * Obj DEVE essere il PRIMO campo di ogni oggetto gestito: cosi' un Array* o un
 * Env* punta allo stesso indirizzo del suo Obj*, e possiamo passare da uno
 * all'altro con un semplice cast.
 */

typedef enum {
    OBJ_ARRAY,
    OBJ_ENV,
    OBJ_STRING,
    OBJ_PROV     /* nodo di provenienza (trace/why), vedi prov.h */
} ObjType;

typedef struct Obj {
    ObjType type;
    unsigned char marked;   /* bit di marcatura (fase mark). 0 = forse morto */
    struct Obj *next;       /* prossimo oggetto nella lista globale          */
} Obj;

/* Dichiarazioni anticipate: gli oggetti veri sono definiti in value.h / env.h
 * / prov.h. */
struct Array;
struct Env;
struct ObjString;
struct Prov;

/* Ciclo di vita del collector. */
void gc_init(void);       /* azzera lo stato: prima di eseguire un programma */
void gc_free_all(void);   /* libera TUTTI gli oggetti: a fine programma      */

/* Allocazioni gestite dal GC: creano l'oggetto, ne impostano l'header e lo
 * agganciano alla lista globale. Da qui in poi la sua memoria e' del GC. */
struct Array     *gc_new_array(void);
struct Env       *gc_new_env(void);
struct ObjString *gc_new_string(const char *chars, int length);
struct Prov      *gc_new_prov(void);   /* nasce vuoto: lo riempie eval.c */

/* Registra la funzione che marca le RADICI (chi e' sicuramente vivo). La chiama
 * eval.c passando una funzione che marca gli ambienti globale e corrente, ecc. */
void gc_set_mark_roots(void (*fn)(void));

/* Usate dalla funzione-radici per marcare un oggetto/ambiente vivo. */
void gc_mark_object(Obj *o);
void gc_mark_env(struct Env *env);

/* Radici temporanee: proteggono un oggetto vivo solo in una variabile C durante
 * il calcolo di un'espressione. Sempre in coppia push/pop (o pop(n)). Accetta
 * anche NULL (ignorato), cosi' si puo' proteggere un Value "qualunque". */
void gc_push_temp(Obj *o);
void gc_pop_temp(int n);

/* Chiamata ai "confini sicuri" (tra un'istruzione e l'altra, fuori dalle
 * chiamate): raccoglie se siamo oltre la soglia di memoria. */
void gc_maybe_collect(void);

/* Aggiorna il contatore dei byte vivi quando cresce un buffer esterno (es. gli
 * elementi di un array). Serve a tenere precisa la soglia di raccolta. */
void gc_count_bytes(size_t n);

#endif /* GC_H */
