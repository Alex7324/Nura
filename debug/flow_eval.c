/* Valutazione e liberazione narranti (post-order). Solo didattica. */
#include "ast.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>   /* fmod, per l'operatore % */

static int d = 0;
static void ind(void) { for (int i = 0; i < d; i++) printf("|  "); }

static const char *op_str(TokenType op) {
    switch (op) {
        case TOKEN_PLUS: return "+"; case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*"; case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_BANG: return "!";
        case TOKEN_EQ: return "=="; case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<"; case TOKEN_LE: return "<=";
        case TOKEN_GT: return ">"; case TOKEN_GE: return ">=";
        case TOKEN_AND: return "&&"; case TOKEN_OR: return "||";
        default: return "?";
    }
}

static double ev(Expr *e) {
    switch (e->type) {
        case EXPR_NUMBER:
            ind(); printf("valuto foglia %g -> %g\n", e->as.number.value, e->as.number.value);
            return e->as.number.value;
        case EXPR_UNARY: {
            const char *s = op_str(e->as.unary.op);
            ind(); printf("nodo (%s): scendo nell'operando\n", s); d++;
            double r = ev(e->as.unary.right); d--;
            double res;
            if (e->as.unary.op == TOKEN_BANG) {
                if (r != 0) res = 0; else res = 1;
            } else {
                res = -r;
            }
            ind(); printf("nodo (%s): %s(%g) = %g\n", s, s, r, res);
            return res;
        }
        case EXPR_BINARY: {
            const char *s = op_str(e->as.binary.op);
            ind(); printf("nodo (%s): scendo a SINISTRA\n", s); d++;
            double l = ev(e->as.binary.left); d--;
            ind(); printf("nodo (%s): scendo a DESTRA\n", s); d++;
            double r = ev(e->as.binary.right); d--;
            double res = 0; TokenType op = e->as.binary.op;
            if (op==TOKEN_PLUS) res=l+r; else if (op==TOKEN_MINUS) res=l-r;
            else if (op==TOKEN_STAR) res=l*r; else if (op==TOKEN_SLASH) res=l/r;
            else if (op==TOKEN_PERCENT) res=fmod(l,r);
            else if (op==TOKEN_EQ) res=(l==r); else if (op==TOKEN_NEQ) res=(l!=r);
            else if (op==TOKEN_LT) res=(l<r); else if (op==TOKEN_LE) res=(l<=r);
            else if (op==TOKEN_GT) res=(l>r); else if (op==TOKEN_GE) res=(l>=r);
            ind(); printf("nodo (%s): %g %s %g = %g\n", s, l, s, r, res);
            return res;
        }
        case EXPR_LOGICAL: {
            const char *s = op_str(e->as.logical.op);
            ind(); printf("nodo (%s): valuto il SINISTRO (corto circuito)\n", s); d++;
            double l = ev(e->as.logical.left); d--;
            if (e->as.logical.op == TOKEN_OR && l != 0) {
                ind(); printf("nodo (%s): sinistro vero -> NON valuto il destro -> 1\n", s);
                return 1;
            }
            if (e->as.logical.op == TOKEN_AND && l == 0) {
                ind(); printf("nodo (%s): sinistro falso -> NON valuto il destro -> 0\n", s);
                return 0;
            }
            ind(); printf("nodo (%s): valuto anche il DESTRO\n", s); d++;
            double r = ev(e->as.logical.right); d--;
            double res; if (r != 0) res = 1; else res = 0;
            ind(); printf("nodo (%s): risultato = %g\n", s, res);
            return res;
        }
    }
    return 0;
}

double eval_expression(Expr *expr, int *had_error) {
    *had_error = 0;
    return ev(expr);
}

/* liberazione narrante (post-order come ast_free) */
void flow_free(Expr *e) {
    if (e == NULL) return;
    if (e->type == EXPR_UNARY) flow_free(e->as.unary.right);
    else if (e->type == EXPR_BINARY || e->type == EXPR_LOGICAL) { flow_free(e->as.binary.left); flow_free(e->as.binary.right); }
    if (e->type == EXPR_NUMBER) { ind(); printf("libero foglia %g\n", e->as.number.value); }
    else {
        TokenType op;
        if (e->type == EXPR_UNARY) op = e->as.unary.op;
        else                       op = e->as.binary.op;
        ind(); printf("libero nodo (%s)\n", op_str(op));
    }
    free(e);
}
