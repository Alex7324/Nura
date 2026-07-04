#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"

/* Modalita' Fase 1: stampa solo il flusso di token. */
static void dump_tokens(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);
    for (;;) {
        Token token = lexer_next_token(&lexer);
        printf("  %-11s '%.*s'", token_type_name(token.type),
               token.length, token.start);
        if (token.type == TOKEN_NUMBER) printf("   -> valore = %g", token.number);
        printf("   [riga %d]\n", token.line);
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) break;
    }
}

/* Modalita' Fase 2+3: costruisce l'AST, lo stampa e lo VALUTA. */
static void dump_ast(const char *source) {
    int had_error = 0;
    Expr *tree = parse_expression_source(source, &had_error);

    if (had_error) {
        printf("\n(l'albero non e' valido a causa dell'errore qui sopra)\n");
        ast_free(tree);
        return;
    }

    printf("Forma compatta:  ");
    ast_print_compact(tree);
    printf("\n\nAlbero:\n");
    ast_print_tree(tree);

    /* Fase 3: valutazione */
    int rt_error = 0;
    double result = eval_expression(tree, &rt_error);
    if (!rt_error) {
        printf("\nRisultato: %g\n", result);
    }

    ast_free(tree);  /* restituiamo la memoria: niente leak */
}

int main(int argc, char **argv) {
    /* Uso:
     *   piccolo "1 + 2 * 3"            -> costruisce e stampa l'AST
     *   piccolo --tokens "1 + 2 * 3"   -> stampa i token (Fase 1)
     */
    if (argc > 1 && strcmp(argv[1], "--tokens") == 0) {
        const char *source = (argc > 2) ? argv[2] : "1 + 2 * 3";
        printf("Sorgente: \"%s\"\n\n", source);
        dump_tokens(source);
        return 0;
    }

    const char *source = (argc > 1) ? argv[1] : "1 + 2 * 3";
    printf("Sorgente: \"%s\"\n\n", source);
    dump_ast(source);
    return 0;
}
