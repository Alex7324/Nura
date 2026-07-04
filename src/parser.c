#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Meccanica di base                                                 */
/* ------------------------------------------------------------------ */

static void error_at(Parser *p, Token token, const char *message) {
    if (p->had_error) return;
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

static void advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);
}

static int check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static int match(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

static void consume(Parser *p, TokenType type, const char *message) {
    if (check(p, type)) { advance(p); return; }
    error_at(p, p->current, message);
}

/* ------------------------------------------------------------------ */
/*  Grammatica delle ESPRESSIONI                                      */
/* ------------------------------------------------------------------ */

static Expr *expression(Parser *p);
static Expr *assignment(Parser *p);
static Expr *equality(Parser *p);
static Expr *comparison(Parser *p);
static Expr *term(Parser *p);
static Expr *factor(Parser *p);
static Expr *unary(Parser *p);
static Expr *primary(Parser *p);

/* expression -> assignment  (l'assegnamento e' l'operazione piu' debole) */
static Expr *expression(Parser *p) {
    return assignment(p);
}

/*
 * assignment -> IDENTIFIER "=" assignment | equality
 *
 * Trucco classico: leggiamo la parte sinistra come una normale espressione
 * (equality). Se poi troviamo un '=', controlliamo che il sinistro sia
 * DAVVERO una variabile (un bersaglio valido); in tal caso costruiamo un
 * nodo di assegnamento. La ricorsione su assignment lo rende associativo
 * a destra: a = b = 1  diventa  a = (b = 1).
 */
static Expr *assignment(Parser *p) {
    Expr *left = equality(p);

    if (check(p, TOKEN_ASSIGN)) {
        advance(p);                       /* consuma '=' */
        Expr *value = assignment(p);      /* il valore (a destra) */

        if (left->type == EXPR_VARIABLE) {
            const char *name = left->as.variable.name;
            Expr *node = ast_assign(name, (int)strlen(name), value);
            ast_free(left);               /* non serve piu' il nodo variabile */
            return node;
        }

        error_at(p, p->previous, "Bersaglio di assegnamento non valido.");
        ast_free(value);
        return left;
    }

    return left;
}

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

static Expr *unary(Parser *p) {
    if (check(p, TOKEN_MINUS)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        return ast_unary(op, right);
    }
    return primary(p);
}

/* primary -> NUMBER | IDENTIFIER | "(" expression ")" */
static Expr *primary(Parser *p) {
    if (check(p, TOKEN_NUMBER)) {
        double value = p->current.number;
        advance(p);
        return ast_number(value);
    }

    /* NUOVO: un identificatore da solo e' la LETTURA di una variabile. */
    if (check(p, TOKEN_IDENTIFIER)) {
        Expr *var = ast_variable(p->current.start, p->current.length);
        advance(p);
        return var;
    }

    if (match(p, TOKEN_LPAREN)) {
        Expr *inner = expression(p);
        consume(p, TOKEN_RPAREN, "Mi aspettavo ')' per chiudere la parentesi.");
        return inner;
    }

    error_at(p, p->current, "Mi aspettavo un numero, un nome o una '('.");
    advance(p);
    return ast_number(0);
}

/* ------------------------------------------------------------------ */
/*  Grammatica delle ISTRUZIONI                                       */
/* ------------------------------------------------------------------ */

static Stmt *statement(Parser *p);
static Stmt *var_declaration(Parser *p);
static Stmt *print_statement(Parser *p);
static Stmt *expression_statement(Parser *p);

/* statement -> varDecl | printStmt | exprStmt */
static Stmt *statement(Parser *p) {
    if (match(p, TOKEN_VAR))   return var_declaration(p);
    if (match(p, TOKEN_PRINT)) return print_statement(p);
    return expression_statement(p);
}

/* varDecl -> "var" IDENTIFIER "=" expression ";"   ('var' gia' consumato) */
static Stmt *var_declaration(Parser *p) {
    Token name = p->current;   /* lo salvo prima di consumarlo */
    consume(p, TOKEN_IDENTIFIER, "Mi aspettavo un nome di variabile dopo 'var'.");
    consume(p, TOKEN_ASSIGN, "Mi aspettavo '=' dopo il nome della variabile.");
    Expr *initializer = expression(p);
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo la dichiarazione.");
    return stmt_var(name.start, name.length, initializer);
}

/* printStmt -> "print" expression ";"   ('print' gia' consumato) */
static Stmt *print_statement(Parser *p) {
    Expr *value = expression(p);
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo il valore di 'print'.");
    return stmt_print(value);
}

/* exprStmt -> expression ";" */
static Stmt *expression_statement(Parser *p) {
    Expr *expr = expression(p);
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo l'espressione.");
    return stmt_expr(expr);
}

/* ------------------------------------------------------------------ */
/*  Punti di ingresso pubblici                                        */
/* ------------------------------------------------------------------ */

Expr *parse_expression_source(const char *source, int *had_error) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;
    advance(&parser);
    Expr *tree = expression(&parser);
    consume(&parser, TOKEN_EOF, "Testo in piu' dopo l'espressione.");
    *had_error = parser.had_error;
    return tree;
}

/* program -> statement* EOF */
void parse_program(const char *source, Program *program, int *had_error) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;
    advance(&parser);            /* primer */

    program_init(program);
    while (!check(&parser, TOKEN_EOF)) {
        Stmt *s = statement(&parser);
        program_add(program, s);
        if (parser.had_error) break;   /* ci fermiamo al primo errore */
    }

    *had_error = parser.had_error;
}
