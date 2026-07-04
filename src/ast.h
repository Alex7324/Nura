#ifndef AST_H
#define AST_H

#include "token.h"

/*
 * AST = Abstract Syntax Tree, l'albero di sintassi.
 *
 * Ogni nodo dell'albero e' una "espressione" (Expr). In questa fase ne
 * abbiamo di tre forme:
 *
 *   - NUMBER : una foglia, contiene solo un valore   es. 42
 *   - UNARY  : un operatore con UN operando           es. -x
 *   - BINARY : un operatore con DUE operandi          es. a + b
 *
 * La struttura ad albero cattura da sola le precedenze: chi sta piu' in
 * basso viene valutato prima. Nota che NON esiste un nodo "parentesi":
 * le parentesi servono solo a dare forma all'albero mentre lo costruiamo,
 * poi spariscono (ne riparliamo nel parser).
 */

typedef enum {
    EXPR_NUMBER,
    EXPR_UNARY,
    EXPR_BINARY
} ExprType;

/* Dichiarazione anticipata: un Expr contiene puntatori ad altri Expr,
 * quindi il tipo deve poter riferire se stesso. */
typedef struct Expr Expr;

struct Expr {
    ExprType type;
    union {
        /* valido se type == EXPR_NUMBER */
        struct {
            double value;
        } number;

        /* valido se type == EXPR_UNARY   (es. -right) */
        struct {
            TokenType op;    /* quale operatore: TOKEN_MINUS, ... */
            Expr *right;     /* l'operando */
        } unary;

        /* valido se type == EXPR_BINARY  (es. left op right) */
        struct {
            TokenType op;    /* TOKEN_PLUS, TOKEN_STAR, TOKEN_LE, ... */
            Expr *left;
            Expr *right;
        } binary;
    } as;
};

/*
 * Costruttori: allocano un nodo con malloc e lo riempiono.
 * Ritornano un Expr* che diventera' un pezzo dell'albero.
 */
Expr *ast_number(double value);
Expr *ast_unary(TokenType op, Expr *right);
Expr *ast_binary(TokenType op, Expr *left, Expr *right);

/* Libera ricorsivamente un albero (nodo + tutti i figli). */
void ast_free(Expr *expr);

/* Stampa l'albero in due modi, per capirne la struttura:
 *  - forma compatta su una riga:  (* (+ 1 2) 3)
 *  - albero indentato verticale. */
void ast_print_compact(Expr *expr);
void ast_print_tree(Expr *expr);

#endif /* AST_H */
