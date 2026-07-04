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
        struct { char *name; } variable;
        struct { char *name; Expr *value; } assign;
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
/*  PROGRAMMA: una lista (array dinamico) di istruzioni               */
/*  Definito PRIMA di Stmt perche' un blocco { } riusa questa lista.  */
/* ================================================================== */

typedef struct Stmt Stmt;   /* dichiarazione anticipata */

typedef struct {
    Stmt **statements;   /* array di puntatori a Stmt */
    int count;
    int capacity;
} Program;

void program_init(Program *program);
void program_add(Program *program, Stmt *stmt);
void program_free(Program *program);

/* ================================================================== */
/*  ISTRUZIONI (statement)  (NON producono un valore: fanno qualcosa) */
/* ================================================================== */

typedef enum {
    STMT_VAR,     /* var nome = expr;                 */
    STMT_PRINT,   /* print expr;                      */
    STMT_EXPR,    /* expr;                            */
    STMT_BLOCK,   /* { istruzione* }                  */
    STMT_IF,      /* if (cond) ramo [else ramo]       */
    STMT_WHILE    /* while (cond) corpo               */
} StmtType;

struct Stmt {
    StmtType type;
    union {
        struct { char *name; Expr *initializer; } var;
        struct { Expr *expr; } print;
        struct { Expr *expr; } expr;
        struct { Program body; } block;
        struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
        struct { Expr *condition; Stmt *body; } while_stmt;
    } as;
};

Stmt *stmt_var(const char *name, int length, Expr *initializer);
Stmt *stmt_print(Expr *expr);
Stmt *stmt_expr(Expr *expr);
Stmt *stmt_block(Program body);
Stmt *stmt_if(Expr *condition, Stmt *then_branch, Stmt *else_branch);
Stmt *stmt_while(Expr *condition, Stmt *body);
void stmt_free(Stmt *stmt);

#endif /* AST_H */
