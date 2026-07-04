#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "pause.h"

void flow_free(Expr *e);  /* definita in eval_flow.c */

static void banner(const char *s) {
    printf("\n============================================================\n");
    printf("  %s\n", s);
    printf("============================================================\n");
}

int main(int argc, char **argv) {
    const char *source;
    if (argc > 1) source = argv[1];
    else          source = "1 + 2 * 3";
    printf("SORGENTE: \"%s\"\n", source);

    banner("FASE 1+2: il parser costruisce l'albero, tirando i token dal lexer uno alla volta");
    int err = 0;
    Expr *tree = parse_expression_source(source, &err);
    if (err) { ast_free(tree); return 1; }

    banner("L'ALBERO COSTRUITO");
    ast_print_tree(tree);

    banner("FASE 3: l'evaluator percorre l'albero (post-order) e calcola");
    int rt = 0;
    double result = eval_expression(tree, &rt);
    printf("\n>>> RISULTATO: %g\n", result);

    banner("PULIZIA: si libera l'albero (post-order) e si esce");
    flow_free(tree);
    printf("\nfine. return 0 -> il sistema operativo chiude il processo.\n");
    wait_before_closing(argc);
    return 0;
}
