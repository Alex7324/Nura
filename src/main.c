#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "env.h"
#include "gc.h"

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

/* Esegue un programma sorgente su un ambiente dato.
 * Ritorna 0 se tutto ok, 1 se c'e' stato un errore (sintassi o runtime). */
static int run_with_env(const char *source, Env *env) {
    Program program;
    int had_error = 0;

    parse_program(source, &program, &had_error);
    if (had_error) {
        program_free(&program);
        return 1;
    }

    int rt_error = 0;
    run_program(&program, env, &rt_error);

    program_free(&program);
    if (rt_error) return 1;
    return 0;
}

/* Esegue un programma con un ambiente "usa e getta". */
static int run(const char *source) {
    gc_init();                       /* prepara il garbage collector          */
    Env *env = gc_new_env();         /* l'ambiente globale e' un oggetto GC    */
    int status = run_with_env(source, env);
    gc_free_all();                   /* libera env + ogni array/ambiente creato */
    return status;
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

/* Modalita' interattiva (REPL): legge una riga, la esegue, ripete.
 * Le variabili restano vive tra una riga e l'altra (stesso ambiente).
 *
 * E' anche la risposta al "doppio-clic che chiude subito la finestra":
 * senza argomenti l'interprete entra qui e RESTA in attesa di input,
 * quindi la finestra non si chiude. Usa solo funzioni C standard
 * (fgets/stdin), quindi funziona uguale su Windows, Linux e Mac.
 *
 * Si esce scrivendo 'exit', oppure con la fine dell'input:
 * Ctrl+D su Linux/Mac, Ctrl+Z e Invio su Windows. */
static void repl(void) {
    printf("Nura - modalita' interattiva.\n");
    printf("Scrivi istruzioni terminate da ';' e premi Invio.  Es:  print 1 + 2;\n");
    printf("Per uscire: 'exit'  (oppure Ctrl+D / Ctrl+Z + Invio).\n\n");

    gc_init();
    Env *env = gc_new_env();   /* un solo ambiente: le variabili persistono tra le righe */

    char line[1024];
    for (;;) {
        printf("nura> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) break;   /* fine input */

        if (strcmp(line, "exit\n") == 0 || strcmp(line, "exit\r\n") == 0 ||
            strcmp(line, "exit") == 0) {
            break;
        }

        run_with_env(line, env);   /* eventuali errori vengono stampati da soli */
    }

    gc_free_all();
    printf("\nA presto!\n");
}

int main(int argc, char **argv) {
    /* Uso:
     *   nura                  modalita' interattiva (REPL)
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

    /* Nessun argomento: modalita' interattiva. */
    repl();
    return 0;
}
