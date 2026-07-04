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
        /* token illegale dal lexer: `message` E' gia' la spiegazione del
         * lexer (es. "Stringa non chiusa..."), quindi non aggiungiamo altro. */
    } else {
        fprintf(stderr, " vicino a '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
}

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next_token(&p->lexer);
        if (p->current.type != TOKEN_ERROR) break;
        /* Il lexer ha prodotto un token illegale: il suo messaggio (dentro
         * token.start) spiega il problema. Lo riportiamo SUBITO, poi
         * continuiamo a scandire cosi' il resto del parser non vede l'errore. */
        error_at(p, p->current, p->current.start);
    }
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
static Expr *logic_or(Parser *p);
static Expr *logic_and(Parser *p);
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
    Expr *left = logic_or(p);

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

/* logic_or -> logic_and ( "||" logic_and )*
 * '||' e' l'operatore piu' debole tra i due logici. */
static Expr *logic_or(Parser *p) {
    Expr *left = logic_and(p);
    while (check(p, TOKEN_OR)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = logic_and(p);
        left = ast_logical(op, left, right);
    }
    return left;
}

/* logic_and -> equality ( "&&" equality )*
 * '&&' lega piu' forte di '||' ma piu' debole dei confronti (==, <, ...). */
static Expr *logic_and(Parser *p) {
    Expr *left = equality(p);
    while (check(p, TOKEN_AND)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = equality(p);
        left = ast_logical(op, left, right);
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
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_PERCENT)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

static Expr *unary(Parser *p) {
    if (check(p, TOKEN_MINUS) || check(p, TOKEN_BANG)) {
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

    if (check(p, TOKEN_STRING)) {
        Expr *s = ast_string(p->current.start, p->current.length);
        advance(p);
        return s;
    }

    if (check(p, TOKEN_TRUE))  { advance(p); return ast_bool(1); }
    if (check(p, TOKEN_FALSE)) { advance(p); return ast_bool(0); }

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
static Stmt *block_statement(Parser *p);
static Stmt *if_statement(Parser *p);
static Stmt *while_statement(Parser *p);

/* statement -> varDecl | printStmt | ifStmt | whileStmt | block | exprStmt */
static Stmt *statement(Parser *p) {
    if (match(p, TOKEN_VAR))    return var_declaration(p);
    if (match(p, TOKEN_PRINT))  return print_statement(p);
    if (match(p, TOKEN_IF))     return if_statement(p);
    if (match(p, TOKEN_WHILE))  return while_statement(p);
    if (match(p, TOKEN_LBRACE)) return block_statement(p);
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

/* block -> "{" statement* "}"   ('{' gia' consumato)
 * Raccoglie le istruzioni finche' non trova '}' (o la fine). */
static Stmt *block_statement(Parser *p) {
    Program body;
    program_init(&body);
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        program_add(&body, statement(p));
        if (p->had_error) break;
    }
    consume(p, TOKEN_RBRACE, "Mi aspettavo '}' per chiudere il blocco.");
    return stmt_block(body);
}

/* ifStmt -> "if" "(" expression ")" statement ( "else" statement )?
 * ('if' gia' consumato). Il ramo puo' essere un blocco { } o una sola istruzione. */
static Stmt *if_statement(Parser *p) {
    consume(p, TOKEN_LPAREN, "Mi aspettavo '(' dopo 'if'.");
    Expr *condition = expression(p);
    consume(p, TOKEN_RPAREN, "Mi aspettavo ')' dopo la condizione dell'if.");

    Stmt *then_branch = statement(p);
    Stmt *else_branch = NULL;
    if (match(p, TOKEN_ELSE)) {
        else_branch = statement(p);
    }
    return stmt_if(condition, then_branch, else_branch);
}

/* whileStmt -> "while" "(" expression ")" statement   ('while' gia' consumato) */
static Stmt *while_statement(Parser *p) {
    consume(p, TOKEN_LPAREN, "Mi aspettavo '(' dopo 'while'.");
    Expr *condition = expression(p);
    consume(p, TOKEN_RPAREN, "Mi aspettavo ')' dopo la condizione del while.");

    Stmt *body = statement(p);
    return stmt_while(condition, body);
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
