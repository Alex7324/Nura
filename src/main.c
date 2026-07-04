#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "env.h"

/* Modalita' diagnostica: stampa solo il flusso di token (Fase 1). */
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

/* Esegue un programma sorgente: parsing -> esecuzione. */
static void run(const char *source) {
    Program program;
    int had_error = 0;

    parse_program(source, &program, &had_error);

    if (!had_error) {
        Env env;
        env_init(&env);

        int rt_error = 0;
        run_program(&program, &env, &rt_error);

        env_free(&env);
    }

    program_free(&program);
}

int main(int argc, char **argv) {
    /* Uso:
     *   nura "var n = 5; print n * 2;"        -> esegue il programma
     *   nura --tokens "var n = 5;"            -> mostra i token (Fase 1)
     */
    if (argc > 1 && strcmp(argv[1], "--tokens") == 0) {
        const char *source = (argc > 2) ? argv[2] : "var n = 5; print n * 2;";
        printf("Sorgente: \"%s\"\n\n", source);
        dump_tokens(source);
        return 0;
    }

    const char *source = (argc > 1) ? argv[1] : "var n = 5; print n * 2;";
    run(source);
    return 0;
}
