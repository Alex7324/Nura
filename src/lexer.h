#ifndef LEXER_H
#define LEXER_H

#include "token.h"

/*
 * Lo stato del lexer mentre scorre il sorgente.
 *
 * Immagina due "dita" appoggiate sul testo:
 *   - start   punta all'inizio del lessema che stiamo costruendo
 *   - current punta al prossimo carattere ancora da esaminare
 *
 * Tra start e current c'e' il pezzo di testo del token in costruzione.
 * line tiene il conto delle righe, per dare errori sensati piu' avanti.
 */
typedef struct {
    const char *start;
    const char *current;
    int line;
} Lexer;

/* Prepara il lexer a leggere la stringa `source`. */
void lexer_init(Lexer *lexer, const char *source);

/* Estrae e ritorna il prossimo token dal sorgente.
 * A input esaurito ritorna sempre TOKEN_EOF (ogni volta). */
Token lexer_next_token(Lexer *lexer);

#endif /* LEXER_H */
