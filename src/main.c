#include <stdio.h>
#include <stdlib.h>
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

/* Esegue un programma sorgente: parsing -> esecuzione.
 * Ritorna 0 se tutto ok, 1 se c'e' stato un errore (sintassi o runtime). */
static int run(const char *source) {
    Program program;
    int had_error = 0;

    parse_program(source, &program, &had_error);
    if (had_error) {
        program_free(&program);
        return 1;
    }

    Env env;
    env_init(&env);

    int rt_error = 0;
    run_program(&program, &env, &rt_error);

    env_free(&env);
    program_free(&program);

    if (rt_error) return 1;
    return 0;
}

/* Legge tutto il contenuto di un file in una stringa (che il chiamante
 * dovra' liberare). Ritorna NULL se il file non si puo' aprire. */
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 0) { fclose(file); return NULL; }

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) { fclose(file); fprintf(stderr, "Memoria esaurita.\n"); exit(1); }

    size_t read = fread(buffer, 1, (size_t)size, file);
    buffer[read] = '\0';   /* terminatore: da qui in poi e' una stringa C */
    fclose(file);
    return buffer;
}

int main(int argc, char **argv) {
    /* Uso:
     *   nura file.nura        esegue un programma da file
     *   nura -e "codice"      esegue codice inline
     *   nura --tokens "..."   mostra i token (debug)
     */
    if (argc >= 2 && strcmp(argv[1], "--tokens") == 0) {
        const char *source;
        if (argc > 2) source = argv[2];
        else          source = "var n = 5; print n * 2;";
        printf("Sorgente: \"%s\"\n\n", source);
        dump_tokens(source);
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        return run(argv[2]);   /* codice inline esplicito */
    }

    if (argc >= 2) {
        char *source = read_file(argv[1]);
        if (source != NULL) {           /* e' un file: eseguilo */
            int status = run(source);
            free(source);
            return status;
        }
        return run(argv[1]);            /* non e' un file: trattalo come codice inline */
    }

    fprintf(stderr,
        "Uso:\n"
        "  nura file.nura        esegue un programma da file\n"
        "  nura -e \"codice\"      esegue codice inline\n"
        "  nura --tokens \"...\"   mostra i token (debug)\n");
    return 1;
}
