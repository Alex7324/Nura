#include "prov.h"

#include <stdio.h>

/*
 * La stampa dell'albero causale di 'why x;'. Formato:
 *
 *   x vale 140                                   <- lo stampa eval.c (il presente)
 *     perche' (riga 6): x = x * 10               <- nodo: l'assegnazione
 *       dove x valeva 14                         <- dipendenza CON storia...
 *         perche' (riga 5): x = (a * b) + 2      <- ...che si apre ricorsivamente
 *           dove a valeva 3 (riga 1)             <- dipendenza senza storia: foglia
 *           dove b valeva 4 (riga 2)
 *
 * La ricorsione e' limitata per costruzione: le catene sono lunghe al massimo
 * PROV_MAX_DEPTH (il tetto applicato alla CREAZIONE dei nodi, vedi eval.c).
 */

static void print_indent(int n) {
    for (int i = 0; i < n; i++) printf("  ");
}

void prov_print(const Prov *p, int indent) {
    print_indent(indent);
    printf("perche' (riga %d): %s = %s\n", p->line, p->target, p->expr_text);

    for (int i = 0; i < p->dep_count; i++) {
        const ProvDep *d = &p->deps[i];
        print_indent(indent + 1);
        printf("dove %s valeva %s", d->name, d->val_text);

        if (d->link != NULL) {
            /* La dipendenza ha una storia: la riga sta nel nodo che segue. */
            printf("\n");
            prov_print(d->link, indent + 2);
        } else {
            /* Foglia: la storia finisce qui. */
            if (d->line > 0)   printf(" (riga %d)", d->line);
            if (d->truncated)  printf(" (storia piu' vecchia dimenticata)");
            printf("\n");
        }
    }
}
