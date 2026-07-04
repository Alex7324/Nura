#ifndef AST_H
#define AST_H

#include "token.h"

/* ================================================================== */
/*  ESPRESSIONI  (producono un valore)                                */
/* ================================================================== */

typedef enum {
    EXPR_NUMBER,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_VARIABLE,   /* lettura di una variabile:   n            */
    EXPR_ASSIGN      /* assegnamento:               n = <expr>   */
} ExprType;

typedef struct Expr Expr;

struct Expr {
    ExprType type;
    union {
        struct { double value; } number;
        struct { TokenType op; Expr *right; } unary;
        struct { TokenType op; Expr *left; Expr *right; } binary;
        struct { char *name; } variable;              /* nome (copia posseduta) */
        struct { char *name; Expr *value; } assign;   /* nome = value           */
    } as;
};

Expr *ast_number(double value);
Expr *ast_unary(TokenType op, Expr *right);
Expr *ast_binary(TokenType op, Expr *left, Expr *right);
Expr *ast_variable(const char *name, int length);
Expr *ast_assign(const char *name, int length, Expr *value);
void ast_free(Expr *expr);
void ast_print_compact(Expr *expr);
void ast_print_tree(Expr *expr);

/* ================================================================== */
/*  ISTRUZIONI (statement)  (NON producono un valore: fanno qualcosa) */
/* ================================================================== */

typedef enum {
    STMT_VAR,     /* var nome = expr;   dichiara una variabile   */
    STMT_PRINT,   /* print expr;        stampa un valore         */
    STMT_EXPR     /* expr;              valuta e scarta (es. n = 1;) */
} StmtType;

typedef struct Stmt Stmt;

struct Stmt {
    StmtType type;
    union {
        struct { char *name; Expr *initializer; } var;
        struct { Expr *expr; } print;
        struct { Expr *expr; } expr;
    } as;
};

Stmt *stmt_var(const char *name, int length, Expr *initializer);
Stmt *stmt_print(Expr *expr);
Stmt *stmt_expr(Expr *expr);
void stmt_free(Stmt *stmt);

/* ================================================================== */
/*  PROGRAMMA: una lista (array dinamico) di istruzioni               */
/* ================================================================== */

typedef struct {
    Stmt **statements;   /* array di puntatori a Stmt */
    int count;           /* quante istruzioni ci sono */
    int capacity;        /* quanto e' grande l'array   */
} Program;

void program_init(Program *program);
void program_add(Program *program, Stmt *stmt);
void program_free(Program *program);

#endif /* AST_H */
