#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Costruttori                                                       */
/* ------------------------------------------------------------------ */

/* Alloca un Expr "vuoto" del tipo dato. Se malloc fallisce, usciamo:
 * in un progetto serio si gestirebbe meglio, ma qui teniamo semplice. */
static Expr *new_expr(ExprType type) {
    Expr *e = malloc(sizeof(Expr));
    if (e == NULL) {
        fprintf(stderr, "Memoria esaurita durante il parsing.\n");
        exit(1);
    }
    e->type = type;
    return e;
}

Expr *ast_number(double value) {
    Expr *e = new_expr(EXPR_NUMBER);
    e->as.number.value = value;
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

/* ------------------------------------------------------------------ */
/*  Liberazione della memoria                                         */
/* ------------------------------------------------------------------ */

/*
 * Per liberare un albero bisogna prima liberare i FIGLI, poi il nodo.
 * E' un attraversamento "post-order": e' lo stesso schema ricorsivo che
 * usa un albero binario. Un numero (foglia) non ha figli: si libera e basta.
 */
void ast_free(Expr *expr) {
    if (expr == NULL) return;
    switch (expr->type) {
        case EXPR_NUMBER:
            break;  /* nessun figlio */
        case EXPR_UNARY:
            ast_free(expr->as.unary.right);
            break;
        case EXPR_BINARY:
            ast_free(expr->as.binary.left);
            ast_free(expr->as.binary.right);
            break;
    }
    free(expr);
}

/* ------------------------------------------------------------------ */
/*  Stampa                                                            */
/* ------------------------------------------------------------------ */

/* Il simbolo testuale di un operatore. */
static const char *op_str(TokenType op) {
    switch (op) {
        case TOKEN_PLUS:  return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR:  return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_EQ:    return "==";
        case TOKEN_NEQ:   return "!=";
        case TOKEN_LT:    return "<";
        case TOKEN_LE:    return "<=";
        case TOKEN_GT:    return ">";
        case TOKEN_GE:    return ">=";
        default:          return "?";
    }
}

/* --- forma compatta: (* (+ 1 2) 3) --- */
void ast_print_compact(Expr *expr) {
    switch (expr->type) {
        case EXPR_NUMBER:
            printf("%g", expr->as.number.value);
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
    }
}

/* --- albero indentato verticale ---
 * L'etichetta di un nodo: il valore se e' un numero, l'operatore altrimenti. */
static void node_label(Expr *expr, char *buf, size_t n) {
    switch (expr->type) {
        case EXPR_NUMBER: snprintf(buf, n, "%g", expr->as.number.value); break;
        case EXPR_UNARY:  snprintf(buf, n, "(%s)", op_str(expr->as.unary.op)); break;
        case EXPR_BINARY: snprintf(buf, n, "(%s)", op_str(expr->as.binary.op)); break;
    }
}

/*
 * `prefix`  = la colonna di "tubi" ereditata dagli antenati
 * `is_last` = questo nodo e' l'ultimo figlio del suo genitore?
 * `is_root` = e' la radice (niente ramo davanti)?
 */
static void print_tree(Expr *expr, const char *prefix, int is_last, int is_root) {
    char label[32];
    node_label(expr, label, sizeof(label));

    if (is_root) {
        printf("%s\n", label);
    } else {
        printf("%s%s%s\n", prefix, is_last ? "`-- " : "|-- ", label);
    }

    /* Prefisso da passare ai figli: se io ero l'ultimo, sotto di me non serve
     * piu' il tubo verticale; altrimenti lo continuo. */
    char child_prefix[256];
    if (is_root) {
        child_prefix[0] = '\0';
    } else {
        snprintf(child_prefix, sizeof(child_prefix), "%s%s",
                 prefix, is_last ? "    " : "|   ");
    }

    if (expr->type == EXPR_UNARY) {
        print_tree(expr->as.unary.right, child_prefix, 1, 0);
    } else if (expr->type == EXPR_BINARY) {
        print_tree(expr->as.binary.left,  child_prefix, 0, 0);
        print_tree(expr->as.binary.right, child_prefix, 1, 0);
    }
}

void ast_print_tree(Expr *expr) {
    print_tree(expr, "", 1, 1);
}
