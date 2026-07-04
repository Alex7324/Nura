#ifndef AST_H
#define AST_H

#include "token.h"

/* ================================================================== */
/*  ESPRESSIONI  (producono un valore)                                */
/* ================================================================== */

typedef enum {
    EXPR_NUMBER,
    EXPR_BOOL,       /* true / false                             */
    EXPR_STRING,     /* "ciao"                                   */
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_LOGICAL,    /* a && b  ,  a || b   (con corto circuito)  */
    EXPR_VARIABLE,   /* lettura di una variabile:   n            */
    EXPR_ASSIGN,     /* assegnamento:               n = <expr>   */
    EXPR_CALL        /* chiamata di funzione:       f(a, b)      */
} ExprType;

typedef struct Expr Expr;

struct Expr {
    ExprType type;
    union {
        struct { double value; } number;
        struct { int value; } boolean;                /* 0 = false, 1 = true */
        struct { char *value; } string;               /* testo, posseduto dall'AST */
        struct { TokenType op; Expr *right; } unary;
        struct { TokenType op; Expr *left; Expr *right; } binary;
        struct { TokenType op; Expr *left; Expr *right; } logical;   /* && oppure || */
        struct { char *name; } variable;
        struct { char *name; Expr *value; } assign;
        struct { Expr *callee; Expr **args; int arg_count; } call;  /* f(a, b) */
    } as;
};

Expr *ast_number(double value);
Expr *ast_bool(int value);
Expr *ast_string(const char *chars, int length);
Expr *ast_unary(TokenType op, Expr *right);
Expr *ast_binary(TokenType op, Expr *left, Expr *right);
Expr *ast_logical(TokenType op, Expr *left, Expr *right);
Expr *ast_variable(const char *name, int length);
Expr *ast_assign(const char *name, int length, Expr *value);
Expr *ast_call(Expr *callee, Expr **args, int arg_count);
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
    STMT_WHILE,   /* while (cond) corpo               */
    STMT_FUN,     /* fun nome(params) { corpo }       */
    STMT_RETURN   /* return expr;                     */
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
        struct { char *name; char **params; int param_count; Stmt *body; } function;
        struct { Expr *value; } ret;   /* return: value puo' essere NULL */
    } as;
};

Stmt *stmt_var(const char *name, int length, Expr *initializer);
Stmt *stmt_print(Expr *expr);
Stmt *stmt_expr(Expr *expr);
Stmt *stmt_block(Program body);
Stmt *stmt_if(Expr *condition, Stmt *then_branch, Stmt *else_branch);
Stmt *stmt_while(Expr *condition, Stmt *body);
Stmt *stmt_fun(const char *name, int name_length, char **params, int param_count, Stmt *body);
Stmt *stmt_return(Expr *value);
void stmt_free(Stmt *stmt);

#endif /* AST_H */
