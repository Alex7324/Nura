#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helper                                                            */
/* ------------------------------------------------------------------ */

static Expr *new_expr(ExprType type) {
    Expr *e = malloc(sizeof(Expr));
    if (e == NULL) { fprintf(stderr, "Memoria esaurita durante il parsing.\n"); exit(1); }
    e->type = type;
    return e;
}

/* Copia un lessema (start..start+length) in una stringa nuova con '\0'.
 * Serve perche' il token punta dentro il sorgente e non e' terminato:
 * l'albero, invece, deve possedere una sua stringa vera. */
static char *copy_name(const char *start, int length) {
    char *s = malloc((size_t)length + 1);
    if (s == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(s, start, (size_t)length);
    s[length] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */
/*  Costruttori di espressioni                                        */
/* ------------------------------------------------------------------ */

Expr *ast_number(double value) {
    Expr *e = new_expr(EXPR_NUMBER);
    e->as.number.value = value;
    return e;
}

Expr *ast_bool(int value) {
    Expr *e = new_expr(EXPR_BOOL);
    e->as.boolean.value = value;
    return e;
}

Expr *ast_string(const char *chars, int length) {
    Expr *e = new_expr(EXPR_STRING);
    e->as.string.value = copy_name(chars, length);   /* copia posseduta dall'AST */
    return e;
}

Expr *ast_unary(TokenType op, Expr *right) {
    Expr *e = new_expr(EXPR_UNARY);
    e->as.unary.op = op;
    e->as.unary.right = right;
    return e;
}

Expr *ast_binary(TokenType op, Expr *left, Expr *right) {
    Expr *e = new_expr(EXPR_BINARY);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    return e;
}

Expr *ast_logical(TokenType op, Expr *left, Expr *right) {
    Expr *e = new_expr(EXPR_LOGICAL);
    e->as.logical.op = op;
    e->as.logical.left = left;
    e->as.logical.right = right;
    return e;
}

Expr *ast_variable(const char *name, int length) {
    Expr *e = new_expr(EXPR_VARIABLE);
    e->as.variable.name = copy_name(name, length);
    return e;
}

Expr *ast_assign(const char *name, int length, Expr *value) {
    Expr *e = new_expr(EXPR_ASSIGN);
    e->as.assign.name = copy_name(name, length);
    e->as.assign.value = value;
    return e;
}

Expr *ast_call(Expr *callee, Expr **args, int arg_count) {
    Expr *e = new_expr(EXPR_CALL);
    e->as.call.callee = callee;
    e->as.call.args = args;          /* prende possesso dell'array di argomenti */
    e->as.call.arg_count = arg_count;
    return e;
}

Expr *ast_array(Expr **elements, int count) {
    Expr *e = new_expr(EXPR_ARRAY);
    e->as.array.elements = elements;   /* prende possesso dell'array di elementi */
    e->as.array.count = count;
    return e;
}

Expr *ast_index(Expr *array, Expr *index) {
    Expr *e = new_expr(EXPR_INDEX);
    e->as.index.array = array;
    e->as.index.index = index;
    return e;
}

Expr *ast_index_set(Expr *array, Expr *index, Expr *value) {
    Expr *e = new_expr(EXPR_INDEX_SET);
    e->as.index_set.array = array;
    e->as.index_set.index = index;
    e->as.index_set.value = value;
    return e;
}

/* ------------------------------------------------------------------ */
/*  Liberazione (post-order)                                          */
/* ------------------------------------------------------------------ */

void ast_free(Expr *expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case EXPR_NUMBER:
            break;
        case EXPR_BOOL:
            break;
        case EXPR_STRING:
            free(expr->as.string.value);
            break;
        case EXPR_UNARY:
            ast_free(expr->as.unary.right);
            break;
        case EXPR_BINARY:
            ast_free(expr->as.binary.left);
            ast_free(expr->as.binary.right);
            break;
        case EXPR_LOGICAL:
            ast_free(expr->as.logical.left);
            ast_free(expr->as.logical.right);
            break;
        case EXPR_VARIABLE:
            free(expr->as.variable.name);   /* liberiamo anche la stringa del nome */
            break;
        case EXPR_ASSIGN:
            free(expr->as.assign.name);
            ast_free(expr->as.assign.value);
            break;
        case EXPR_CALL:
            ast_free(expr->as.call.callee);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                ast_free(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            break;
        case EXPR_ARRAY:
            for (int i = 0; i < expr->as.array.count; i++) {
                ast_free(expr->as.array.elements[i]);
            }
            free(expr->as.array.elements);
            break;
        case EXPR_INDEX:
            ast_free(expr->as.index.array);
            ast_free(expr->as.index.index);
            break;
        case EXPR_INDEX_SET:
            ast_free(expr->as.index_set.array);
            ast_free(expr->as.index_set.index);
            ast_free(expr->as.index_set.value);
            break;
    }
    free(expr);
}

/* ------------------------------------------------------------------ */
/*  Stampa                                                            */
/* ------------------------------------------------------------------ */

static const char *op_str(TokenType op) {
    switch (op) {
        case TOKEN_PLUS:  return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR:  return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_BANG:  return "!";
        case TOKEN_EQ:    return "==";
        case TOKEN_NEQ:   return "!=";
        case TOKEN_LT:    return "<";
        case TOKEN_LE:    return "<=";
        case TOKEN_GT:    return ">";
        case TOKEN_GE:    return ">=";
        case TOKEN_AND:   return "&&";
        case TOKEN_OR:    return "||";
        default:          return "?";
    }
}

void ast_print_compact(Expr *expr) {
    switch (expr->type) {
        case EXPR_NUMBER:
            printf("%g", expr->as.number.value);
            break;
        case EXPR_BOOL:
            if (expr->as.boolean.value) printf("true");
            else                        printf("false");
            break;
        case EXPR_STRING:
            printf("\"%s\"", expr->as.string.value);
            break;
        case EXPR_UNARY:
            printf("(%s ", op_str(expr->as.unary.op));
            ast_print_compact(expr->as.unary.right);
            printf(")");
            break;
        case EXPR_BINARY:
            printf("(%s ", op_str(expr->as.binary.op));
            ast_print_compact(expr->as.binary.left);
            printf(" ");
            ast_print_compact(expr->as.binary.right);
            printf(")");
            break;
        case EXPR_LOGICAL:
            printf("(%s ", op_str(expr->as.logical.op));
            ast_print_compact(expr->as.logical.left);
            printf(" ");
            ast_print_compact(expr->as.logical.right);
            printf(")");
            break;
        case EXPR_VARIABLE:
            printf("%s", expr->as.variable.name);
            break;
        case EXPR_ASSIGN:
            printf("(= %s ", expr->as.assign.name);
            ast_print_compact(expr->as.assign.value);
            printf(")");
            break;
        case EXPR_CALL:
            printf("(call ");
            ast_print_compact(expr->as.call.callee);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                printf(" ");
                ast_print_compact(expr->as.call.args[i]);
            }
            printf(")");
            break;
        case EXPR_ARRAY:
            printf("(array");
            for (int i = 0; i < expr->as.array.count; i++) {
                printf(" ");
                ast_print_compact(expr->as.array.elements[i]);
            }
            printf(")");
            break;
        case EXPR_INDEX:
            printf("(index ");
            ast_print_compact(expr->as.index.array);
            printf(" ");
            ast_print_compact(expr->as.index.index);
            printf(")");
            break;
        case EXPR_INDEX_SET:
            printf("(index-set ");
            ast_print_compact(expr->as.index_set.array);
            printf(" ");
            ast_print_compact(expr->as.index_set.index);
            printf(" ");
            ast_print_compact(expr->as.index_set.value);
            printf(")");
            break;
    }
}

static void node_label(Expr *expr, char *buf, size_t n) {
    switch (expr->type) {
        case EXPR_NUMBER:   snprintf(buf, n, "%g", expr->as.number.value); break;
        case EXPR_BOOL:
            if (expr->as.boolean.value) snprintf(buf, n, "true");
            else                        snprintf(buf, n, "false");
            break;
        case EXPR_STRING:   snprintf(buf, n, "\"%s\"", expr->as.string.value); break;
        case EXPR_UNARY:    snprintf(buf, n, "(%s)", op_str(expr->as.unary.op)); break;
        case EXPR_BINARY:   snprintf(buf, n, "(%s)", op_str(expr->as.binary.op)); break;
        case EXPR_LOGICAL:  snprintf(buf, n, "(%s)", op_str(expr->as.logical.op)); break;
        case EXPR_VARIABLE: snprintf(buf, n, "%s", expr->as.variable.name); break;
        case EXPR_ASSIGN:   snprintf(buf, n, "(= %s)", expr->as.assign.name); break;
        case EXPR_CALL:     snprintf(buf, n, "(call)"); break;
        case EXPR_ARRAY:    snprintf(buf, n, "(array)"); break;
        case EXPR_INDEX:    snprintf(buf, n, "(index)"); break;
        case EXPR_INDEX_SET:snprintf(buf, n, "(index-set)"); break;
    }
}

static void print_tree(Expr *expr, const char *prefix, int is_last, int is_root) {
    char label[64];
    node_label(expr, label, sizeof(label));

    if (is_root) {
        printf("%s\n", label);
    } else {
        const char *ramo;
        if (is_last) ramo = "`-- ";
        else         ramo = "|-- ";
        printf("%s%s%s\n", prefix, ramo, label);
    }

    char child_prefix[256];
    if (is_root) {
        child_prefix[0] = '\0';
    } else {
        const char *pad;
        if (is_last) pad = "    ";
        else         pad = "|   ";
        snprintf(child_prefix, sizeof(child_prefix), "%s%s", prefix, pad);
    }

    if (expr->type == EXPR_UNARY) {
        print_tree(expr->as.unary.right, child_prefix, 1, 0);
    } else if (expr->type == EXPR_BINARY) {
        print_tree(expr->as.binary.left,  child_prefix, 0, 0);
        print_tree(expr->as.binary.right, child_prefix, 1, 0);
    } else if (expr->type == EXPR_LOGICAL) {
        print_tree(expr->as.logical.left,  child_prefix, 0, 0);
        print_tree(expr->as.logical.right, child_prefix, 1, 0);
    } else if (expr->type == EXPR_ASSIGN) {
        print_tree(expr->as.assign.value, child_prefix, 1, 0);
    } else if (expr->type == EXPR_CALL) {
        int last = expr->as.call.arg_count;   /* indice dell'ultimo figlio */
        print_tree(expr->as.call.callee, child_prefix, last == 0, 0);
        for (int i = 0; i < expr->as.call.arg_count; i++) {
            print_tree(expr->as.call.args[i], child_prefix, i == last - 1, 0);
        }
    } else if (expr->type == EXPR_ARRAY) {
        int count = expr->as.array.count;
        for (int i = 0; i < count; i++) {
            print_tree(expr->as.array.elements[i], child_prefix, i == count - 1, 0);
        }
    } else if (expr->type == EXPR_INDEX) {
        print_tree(expr->as.index.array, child_prefix, 0, 0);
        print_tree(expr->as.index.index, child_prefix, 1, 0);
    } else if (expr->type == EXPR_INDEX_SET) {
        print_tree(expr->as.index_set.array, child_prefix, 0, 0);
        print_tree(expr->as.index_set.index, child_prefix, 0, 0);
        print_tree(expr->as.index_set.value, child_prefix, 1, 0);
    }
    /* EXPR_NUMBER e EXPR_VARIABLE sono foglie: nessun figlio */
}

void ast_print_tree(Expr *expr) {
    print_tree(expr, "", 1, 1);
}

/* ------------------------------------------------------------------ */
/*  Istruzioni                                                        */
/* ------------------------------------------------------------------ */

static Stmt *new_stmt(StmtType type) {
    Stmt *s = malloc(sizeof(Stmt));
    if (s == NULL) { fprintf(stderr, "Memoria esaurita durante il parsing.\n"); exit(1); }
    s->type = type;
    return s;
}

Stmt *stmt_var(const char *name, int length, Expr *initializer) {
    Stmt *s = new_stmt(STMT_VAR);
    s->as.var.name = copy_name(name, length);
    s->as.var.initializer = initializer;
    return s;
}

Stmt *stmt_print(Expr *expr) {
    Stmt *s = new_stmt(STMT_PRINT);
    s->as.print.expr = expr;
    return s;
}

Stmt *stmt_expr(Expr *expr) {
    Stmt *s = new_stmt(STMT_EXPR);
    s->as.expr.expr = expr;
    return s;
}

Stmt *stmt_block(Program body) {
    Stmt *s = new_stmt(STMT_BLOCK);
    s->as.block.body = body;   /* il blocco prende possesso della lista */
    return s;
}

Stmt *stmt_if(Expr *condition, Stmt *then_branch, Stmt *else_branch) {
    Stmt *s = new_stmt(STMT_IF);
    s->as.if_stmt.condition = condition;
    s->as.if_stmt.then_branch = then_branch;
    s->as.if_stmt.else_branch = else_branch;   /* puo' essere NULL */
    return s;
}

Stmt *stmt_while(Expr *condition, Stmt *body) {
    Stmt *s = new_stmt(STMT_WHILE);
    s->as.while_stmt.condition = condition;
    s->as.while_stmt.body = body;
    return s;
}

Stmt *stmt_fun(const char *name, int name_length, char **params, int param_count, Stmt *body) {
    Stmt *s = new_stmt(STMT_FUN);
    s->as.function.name = copy_name(name, name_length);
    s->as.function.params = params;   /* prende possesso dell'array di parametri */
    s->as.function.param_count = param_count;
    s->as.function.body = body;
    return s;
}

Stmt *stmt_return(Expr *value) {
    Stmt *s = new_stmt(STMT_RETURN);
    s->as.ret.value = value;   /* puo' essere NULL (return senza valore) */
    return s;
}

void stmt_free(Stmt *stmt) {
    if (stmt == NULL) return;
    switch (stmt->type) {
        case STMT_VAR:
            free(stmt->as.var.name);
            ast_free(stmt->as.var.initializer);
            break;
        case STMT_PRINT:
            ast_free(stmt->as.print.expr);
            break;
        case STMT_EXPR:
            ast_free(stmt->as.expr.expr);
            break;
        case STMT_BLOCK:
            program_free(&stmt->as.block.body);   /* libera tutte le istruzioni del blocco */
            break;
        case STMT_IF:
            ast_free(stmt->as.if_stmt.condition);
            stmt_free(stmt->as.if_stmt.then_branch);
            stmt_free(stmt->as.if_stmt.else_branch);   /* stmt_free gestisce NULL */
            break;
        case STMT_WHILE:
            ast_free(stmt->as.while_stmt.condition);
            stmt_free(stmt->as.while_stmt.body);
            break;
        case STMT_FUN:
            free(stmt->as.function.name);
            for (int i = 0; i < stmt->as.function.param_count; i++) {
                free(stmt->as.function.params[i]);
            }
            free(stmt->as.function.params);
            stmt_free(stmt->as.function.body);
            break;
        case STMT_RETURN:
            ast_free(stmt->as.ret.value);   /* ast_free gestisce NULL */
            break;
    }
    free(stmt);
}

/* ------------------------------------------------------------------ */
/*  Programma: array dinamico di istruzioni                           */
/* ------------------------------------------------------------------ */

void program_init(Program *program) {
    program->statements = NULL;
    program->count = 0;
    program->capacity = 0;
}

/* Aggiunge un'istruzione, facendo crescere l'array quando e' pieno.
 * Strategia classica: quando serve, RADDOPPIA la capacita'. Cosi' in
 * media ogni inserimento costa O(1). */
void program_add(Program *program, Stmt *stmt) {
    if (program->count == program->capacity) {
        int new_capacity;
        if (program->capacity < 8) new_capacity = 8;
        else                       new_capacity = program->capacity * 2;
        Stmt **grown = realloc(program->statements,
                               sizeof(Stmt *) * (size_t)new_capacity);
        if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
        program->statements = grown;
        program->capacity = new_capacity;
    }
    program->statements[program->count++] = stmt;
}

void program_free(Program *program) {
    for (int i = 0; i < program->count; i++) {
        stmt_free(program->statements[i]);
    }
    free(program->statements);
    program->statements = NULL;
    program->count = 0;
    program->capacity = 0;
}
