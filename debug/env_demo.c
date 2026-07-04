/* Visualizzatore dell'AMBIENTE (Fase 4): mostra la tabella hash che
 * associa nome -> valore. Disegna i bucket cosi' si vedono la
 * distribuzione dei nomi e le COLLISIONI (bucket con piu' di una voce,
 * gestite col "chaining", cioe' una lista concatenata per bucket).
 *
 * Non fa parte dell'interprete vero (src/): e' solo una lente per capire
 * come e' fatta dentro la struttura dati delle variabili.
 *
 * Compilazione (dalla cartella principale):
 *   gcc -std=c11 -I src debug/env_demo.c src/env.c -o env_demo
 *   ./env_demo
 */
#include "env.h"
#include <stdio.h>
#include "pause.h"

/* Disegna lo stato interno della tabella: per ogni bucket non vuoto,
 * stampa la catena di voci. */
static void dump(Env *env) {
    printf("--- tabella (count=%d, capacity=%d) ---\n", env->count, ENV_CAPACITY);
    for (int i = 0; i < ENV_CAPACITY; i++) {
        if (env->buckets[i] == NULL) continue;
        printf("  bucket[%2d]:", i);
        for (Entry *e = env->buckets[i]; e != NULL; e = e->next) {
            printf("  ->  %s=%g", e->name, e->value);
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char **argv) {
    (void)argv;   /* non usati: env_demo esegue sempre la stessa demo */
    Env env;
    env_init(&env);

    printf("=== 1) definizioni base (var ...) ===\n");
    env_define(&env, "n", 5);
    env_define(&env, "somma", 100);
    env_define(&env, "i", 0);
    env_define(&env, "risultato", 42);
    dump(&env);

    printf("=== 2) letture ===\n");
    double v;
    if (env_get(&env, "somma", &v)) printf("  somma = %g\n", v);
    if (!env_get(&env, "pippo", &v)) printf("  'pippo' non e' definita\n");

    printf("\n=== 3) ridefinizione (var n = 999): aggiorna, non duplica ===\n");
    env_define(&env, "n", 999);
    env_get(&env, "n", &v);
    printf("  ora n = %g\n", v);

    printf("\n=== 4) assegnamento (n = ...): solo se gia' dichiarata ===\n");
    if (env_assign(&env, "i", 7)) { env_get(&env, "i", &v); printf("  i = %g\n", v); }
    if (!env_assign(&env, "inesistente", 1))
        printf("  non posso assegnare a 'inesistente': non e' dichiarata\n");

    printf("\n=== 5) tante variabili: qui si vedono le COLLISIONI (chaining) ===\n");
    char nome[16];
    for (int k = 0; k < 30; k++) {
        snprintf(nome, sizeof(nome), "v%d", k);
        env_define(&env, nome, k);
    }
    dump(&env);

    env_free(&env);
    wait_before_closing(argc);
    return 0;
}
