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
    EXPR_CALL,       /* chiamata di funzione:       f(a, b)      */
    EXPR_ARRAY,      /* literal di array:           [a, b, c]    */
    EXPR_INDEX,      /* lettura per indice:         arr[i]       */
    EXPR_INDEX_SET   /* scrittura per indice:       arr[i] = v   */
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
        struct { char *name; Expr *value; int line; } assign;   /* line: per trace/why */
        struct { Expr *callee; Expr **args; int arg_count; } call;  /* f(a, b) */
        struct { Expr **elements; int count; } array;              /* [a, b, c] */
        struct { Expr *array; Expr *index; } index;                /* arr[i]    */
        struct { Expr *array; Expr *index; Expr *value; } index_set; /* arr[i] = v */
    } as;
};

Expr *ast_number(double value);
Expr *ast_bool(int value);
Expr *ast_string(const char *chars, int length);
Expr *ast_unary(TokenType op, Expr *right);
Expr *ast_binary(TokenType op, Expr *left, Expr *right);
Expr *ast_logical(TokenType op, Expr *left, Expr *right);
Expr *ast_variable(const char *name, int length);
Expr *ast_assign(const char *name, int length, Expr *value, int line);
Expr *ast_call(Expr *callee, Expr **args, int arg_count);
Expr *ast_array(Expr **elements, int count);
Expr *ast_index(Expr *array, Expr *index);
Expr *ast_index_set(Expr *array, Expr *index, Expr *value);
void ast_free(Expr *expr);
void ast_print_compact(Expr *expr);
void ast_print_tree(Expr *expr);

/* Ricostruisce il TESTO di un'espressione dall'albero (es. "(a * b) + 2").
 * Serve a trace/why per mostrare CHI ha prodotto un valore. Scrive al massimo
 * cap-1 caratteri in buf (tronca con "..." se non ci sta). */
void ast_expr_text(Expr *expr, char *buf, int cap);

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
    STMT_FOR,     /* for (init; cond; incr) corpo     */
    STMT_BREAK,   /* break;                           */
    STMT_CONTINUE,/* continue;                        */
    STMT_FUN,     /* fun nome(params) { corpo }       */
    STMT_RETURN,  /* return expr;                     */
    STMT_RECUR,   /* recur chiamata;  (tail call, TCO) */
    STMT_TRACE,   /* trace x;   (attiva la storia di x) */
    STMT_WHY      /* why x;     (stampa l'albero causale) */
} StmtType;

struct Stmt {
    StmtType type;
    int line;      /* riga del sorgente dove INIZIA l'istruzione (0 = ignota).
                    * La imposta il parser in un punto solo (statement());
                    * l'evaluator la usa per gli errori a runtime. */
    union {
        struct { char *name; Expr *initializer; int line; } var;   /* line: per trace/why */
        struct { Expr *expr; } print;
        struct { Expr *expr; } expr;
        struct { Program body; } block;
        struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
        struct { Expr *condition; Stmt *body; } while_stmt;
        /* for: init, cond e incr possono essere NULL (clausole facoltative) */
        struct { Stmt *initializer; Expr *condition; Expr *increment; Stmt *body; } for_stmt;
        struct { char *name; char **params; int param_count; Stmt *body; } function;
        struct { Expr *value; } ret;   /* return: value puo' essere NULL */
        struct { Expr *call; } recur;  /* recur: call e' sempre un EXPR_CALL */
        struct { char *name; } trace;  /* trace x; */
        struct { char *name; } why;    /* why x;   */
    } as;
};

Stmt *stmt_var(const char *name, int length, Expr *initializer, int line);
Stmt *stmt_print(Expr *expr);
Stmt *stmt_expr(Expr *expr);
Stmt *stmt_block(Program body);
Stmt *stmt_for(Stmt *initializer, Expr *condition, Expr *increment, Stmt *body);
Stmt *stmt_break(void);
Stmt *stmt_continue(void);
Stmt *stmt_if(Expr *condition, Stmt *then_branch, Stmt *else_branch);
Stmt *stmt_while(Expr *condition, Stmt *body);
Stmt *stmt_fun(const char *name, int name_length, char **params, int param_count, Stmt *body);
Stmt *stmt_return(Expr *value);
Stmt *stmt_recur(Expr *call);   /* call deve essere un EXPR_CALL */
Stmt *stmt_trace(const char *name, int length);
Stmt *stmt_why(const char *name, int length);
void stmt_free(Stmt *stmt);

#endif /* AST_H */
