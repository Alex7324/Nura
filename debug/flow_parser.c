/* Copia "narrante" del parser: stessa identica logica di src/parser.c, ma
 * stampa (con indentazione = profondita' della ricorsione) OGNI passo:
 * quando entra in una funzione della grammatica, quando chiede un token al
 * lexer, quando crea un nodo. Serve a VEDERE l'ordine reale. Solo didattica. */
#include "parser.h"
#include <stdio.h>

static int depth = 0;
static void ind(void) { for (int i = 0; i < depth; i++) printf("|  "); }
static void enter(const char *name) { ind(); printf(">> entro in %s()\n", name); depth++; }
static void leave(void) { depth--; }

/* ------- meccanica di base (con narrazione su advance) ------- */

static void error_at(Parser *p, Token token, const char *message) {
    if (p->had_error) return;
    p->had_error = 1;
    fprintf(stderr, "[riga %d] Errore di sintassi: %s\n", token.line, message);
}

static void advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);   /* <-- il lexer lavora QUI, on-demand */
    ind();
    printf("   [LEXER -> token: %s '%.*s']\n",
           token_type_name(p->current.type), p->current.length, p->current.start);
}

static int check(Parser *p, TokenType type) { return p->current.type == type; }
static int match(Parser *p, TokenType type) { if (!check(p,type)) return 0; advance(p); return 1; }
static void consume(Parser *p, TokenType type, const char *msg) {
    if (check(p,type)) { advance(p); return; }
    error_at(p, p->current, msg);
}

static Expr *expression(Parser *p);
static Expr *logic_or(Parser *p);
static Expr *logic_and(Parser *p);
static Expr *equality(Parser *p);
static Expr *comparison(Parser *p);
static Expr *term(Parser *p);
static Expr *factor(Parser *p);
static Expr *unary(Parser *p);
static Expr *primary(Parser *p);

static const char *op_str(TokenType op) {
    switch (op) {
        case TOKEN_PLUS: return "+"; case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*"; case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_AND: return "&&"; case TOKEN_OR: return "||";
        default: return "?";
    }
}

static Expr *expression(Parser *p) { enter("expression"); Expr *e = logic_or(p); leave(); return e; }

static Expr *logic_or(Parser *p) {
    enter("logic_or");
    Expr *left = logic_and(p);
    while (check(p, TOKEN_OR)) {
        TokenType op = p->current.type;
        ind(); printf("   ** logic_or: vedo '||' -> raccolgo il destro\n");
        advance(p);
        Expr *right = logic_and(p);
        left = ast_logical(op, left, right);
        ind(); printf("   ** logic_or: creo nodo (||)\n");
    }
    leave(); return left;
}

static Expr *logic_and(Parser *p) {
    enter("logic_and");
    Expr *left = equality(p);
    while (check(p, TOKEN_AND)) {
        TokenType op = p->current.type;
        ind(); printf("   ** logic_and: vedo '&&' -> raccolgo il destro\n");
        advance(p);
        Expr *right = equality(p);
        left = ast_logical(op, left, right);
        ind(); printf("   ** logic_and: creo nodo (&&)\n");
    }
    leave(); return left;
}

static Expr *equality(Parser *p) {
    enter("equality");
    Expr *left = comparison(p);
    while (check(p, TOKEN_EQ) || check(p, TOKEN_NEQ)) {
        TokenType op = p->current.type; advance(p);
        Expr *right = comparison(p);
        left = ast_binary(op, left, right);
    }
    leave(); return left;
}

static Expr *comparison(Parser *p) {
    enter("comparison");
    Expr *left = term(p);
    while (check(p,TOKEN_LT)||check(p,TOKEN_LE)||check(p,TOKEN_GT)||check(p,TOKEN_GE)) {
        TokenType op = p->current.type; advance(p);
        Expr *right = term(p);
        left = ast_binary(op, left, right);
    }
    leave(); return left;
}

static Expr *term(Parser *p) {
    enter("term");
    Expr *left = factor(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        TokenType op = p->current.type;
        ind(); printf("   ** term: vedo '%s', e' roba mia -> raccolgo il destro\n", op_str(op));
        advance(p);
        Expr *right = factor(p);
        left = ast_binary(op, left, right);
        ind(); printf("   ** term: creo nodo (%s)\n", op_str(op));
    }
    leave(); return left;
}

static Expr *factor(Parser *p) {
    enter("factor");
    Expr *left = unary(p);
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_PERCENT)) {
        TokenType op = p->current.type;
        ind(); printf("   ** factor: vedo '%s', e' roba mia -> raccolgo il destro\n", op_str(op));
        advance(p);
        Expr *right = unary(p);
        left = ast_binary(op, left, right);
        ind(); printf("   ** factor: creo nodo (%s)\n", op_str(op));
    }
    leave(); return left;
}

static Expr *unary(Parser *p) {
    enter("unary");
    if (check(p, TOKEN_MINUS) || check(p, TOKEN_BANG)) {
        TokenType op = p->current.type; advance(p);
        Expr *right = unary(p);
        leave(); return ast_unary(op, right);
    }
    Expr *e = primary(p);
    leave(); return e;
}

static Expr *primary(Parser *p) {
    enter("primary");
    if (check(p, TOKEN_NUMBER)) {
        double value = p->current.number;
        ind(); printf("   ** primary: e' il numero %g -> creo foglia\n", value);
        advance(p);
        leave(); return ast_number(value);
    }
    if (match(p, TOKEN_LPAREN)) {
        ind(); printf("   ** primary: '(' -> ricomincio da expression\n");
        Expr *inner = expression(p);
        consume(p, TOKEN_RPAREN, "Mi aspettavo ')'.");
        leave(); return inner;
    }
    error_at(p, p->current, "Mi aspettavo un numero o una '('.");
    advance(p);
    leave(); return ast_number(0);
}

Expr *parse_expression_source(const char *source, int *had_error) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;
    printf("[PARSER] primer: chiedo il primo token al lexer\n");
    advance(&parser);
    Expr *tree = expression(&parser);
    consume(&parser, TOKEN_EOF, "Testo in piu' dopo l'espressione.");
    *had_error = parser.had_error;
    return tree;
}
