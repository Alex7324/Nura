#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

/*
 * Lo stato del parser.
 *
 * Il parser NON tiene tutti i token in un array: chiede al lexer un token
 * alla volta. Per decidere cosa fare gli basta guardare il token corrente
 * (a volte deve anche ricordare quello appena consumato), quindi ne tiene
 * due sotto mano:
 *   - current  : il prossimo token da esaminare (il "lookahead")
 *   - previous : l'ultimo token consumato (utile per sapere QUALE operatore
 *                abbiamo appena letto)
 */
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int had_error;   /* diventa 1 se incontriamo un errore di sintassi */
} Parser;

/*
 * Analizza `source` come una singola espressione e ne ritorna l'albero.
 * Se c'e' un errore di sintassi, stampa un messaggio e mette *had_error a 1
 * (l'albero ritornato in quel caso non e' affidabile).
 * Il chiamante e' responsabile di liberare l'albero con ast_free().
 */
Expr *parse_expression_source(const char *source, int *had_error);

#endif /* PARSER_H */
