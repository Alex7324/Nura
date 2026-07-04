#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Meccanica di base: muoversi tra i token                           */
/* ------------------------------------------------------------------ */

/* Segnala un errore di sintassi: lo stampa e alza la bandierina.
 * Per ora ci fermiamo al primo errore, non proviamo a "recuperare". */
static void error_at(Parser *p, Token token, const char *message) {
    if (p->had_error) return;  /* evita valanghe di errori a cascata */
    p->had_error = 1;
    fprintf(stderr, "[riga %d] Errore di sintassi", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " a fine input");
    } else if (token.type == TOKEN_ERROR) {
        fprintf(stderr, " (token illegale)");
    } else {
        fprintf(stderr, " vicino a '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
}

/* Consuma il token corrente e chiede al lexer il prossimo.
 * `previous` ricorda quello appena lasciato. */
static void advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);
}

/* Il token corrente e' di questo tipo? (senza consumarlo) */
static int check(Parser *p, TokenType type) {
    return p->current.type == type;
}

/* Se il token corrente e' `type`, consumalo e ritorna 1. Altrimenti 0. */
static int match(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

/* Pretende un certo token; se non c'e', e' un errore di sintassi. */
static void consume(Parser *p, TokenType type, const char *message) {
    if (check(p, type)) {
        advance(p);
        return;
    }
    error_at(p, p->current, message);
}

/* ------------------------------------------------------------------ */
/*  La grammatica, un livello per funzione (dal piu' debole al piu'    */
/*  forte). Ognuna e' dichiarata prima perche' si chiamano a vicenda.  */
/* ------------------------------------------------------------------ */

static Expr *expression(Parser *p);
static Expr *equality(Parser *p);
static Expr *comparison(Parser *p);
static Expr *term(Parser *p);
static Expr *factor(Parser *p);
static Expr *unary(Parser *p);
static Expr *primary(Parser *p);

/* expression -> equality  (il punto di ingresso della grammatica) */
static Expr *expression(Parser *p) {
    return equality(p);
}

/*
 * Tutti i livelli binari hanno LA STESSA forma:
 *
 *   1. leggi un operando chiamando il livello SUPERIORE (piu' forte)
 *   2. FINCHE' vedi uno degli operatori di questo livello:
 *        - ricordati l'operatore
 *        - leggi un altro operando (di nuovo dal livello superiore)
 *        - impacchetta i due in un nodo BINARY, che diventa il nuovo "left"
 *
 * Il while che riusa `left` produce l'associativita' A SINISTRA:
 *   a - b - c   diventa   ((a - b) - c),  non  (a - (b - c)).
 */

/* equality -> comparison ( ( "==" | "!=" ) comparison )* */
static Expr *equality(Parser *p) {
    Expr *left = comparison(p);
    while (check(p, TOKEN_EQ) || check(p, TOKEN_NEQ)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = comparison(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

/* comparison -> term ( ( "<" | "<=" | ">" | ">=" ) term )* */
static Expr *comparison(Parser *p) {
    Expr *left = term(p);
    while (check(p, TOKEN_LT) || check(p, TOKEN_LE) ||
           check(p, TOKEN_GT) || check(p, TOKEN_GE)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = term(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

/* term -> factor ( ( "+" | "-" ) factor )* */
static Expr *term(Parser *p) {
    Expr *left = factor(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = factor(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

/* factor -> unary ( ( "*" | "/" ) unary )* */
static Expr *factor(Parser *p) {
    Expr *left = unary(p);
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

/*
 * unary -> "-" unary | primary
 *
 * Il meno unario e' RICORSIVO su se stesso: cosi' "--5" (meno di meno di 5)
 * e' gestito naturalmente. Se non c'e' un meno, scendiamo a primary.
 * L'associativita' qui e' a destra, ed e' quella giusta per un prefisso.
 */
static Expr *unary(Parser *p) {
    if (check(p, TOKEN_MINUS)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        return ast_unary(op, right);
    }
    return primary(p);
}

/*
 * primary -> NUMBER | "(" expression ")"
 *
 * E' il fondo della scala: le cose "atomiche".
 * La parentesi e' il trucco che permette di forzare le precedenze: dentro
 * "(" ... ")" ricominciamo da expression (il livello piu' debole), cioe'
 * "azzeriamo" la precedenza. Nota che NON creiamo un nodo per la parentesi:
 * il suo unico scopo era far tornare qui la ricorsione; l'albero risultante
 * ha gia' la forma giusta, la parentesi ha finito il suo lavoro.
 */
static Expr *primary(Parser *p) {
    if (check(p, TOKEN_NUMBER)) {
        double value = p->current.number;
        advance(p);
        return ast_number(value);
    }

    if (match(p, TOKEN_LPAREN)) {
        Expr *inner = expression(p);
        consume(p, TOKEN_RPAREN, "Mi aspettavo ')' per chiudere la parentesi.");
        return inner;
    }

    /* Nessuna alternativa valida: errore. Ritorniamo un nodo "0" segnaposto
     * cosi' il resto del programma non deve gestire puntatori NULL. */
    error_at(p, p->current, "Mi aspettavo un numero o una '('.");
    advance(p);  /* consuma il token incriminato per non restare bloccati */
    return ast_number(0);
}

/* ------------------------------------------------------------------ */
/*  Punto di ingresso pubblico                                        */
/* ------------------------------------------------------------------ */

Expr *parse_expression_source(const char *source, int *had_error) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;

    /* "Primer": carichiamo il primo token in current.
     * (previous rimane vuoto finche' non consumiamo qualcosa.) */
    advance(&parser);

    Expr *tree = expression(&parser);

    /* Dopo l'espressione ci aspettiamo la fine dell'input: se avanza altro
     * testo (es. "1 2"), e' un errore. */
    consume(&parser, TOKEN_EOF, "Testo in piu' dopo l'espressione.");

    *had_error = parser.had_error;
    return tree;
}
