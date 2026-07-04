/* nura_run — narratore dell'ESECUZIONE di un programma (Fase 4).
 *
 * A differenza di nura_flow (che mostra come UNA espressione diventa un
 * albero), qui vediamo un PROGRAMMA intero: la lista di istruzioni prodotta
 * dal parser, e poi l'esecuzione passo per passo, con le operazioni
 * sull'AMBIENTE (la tabella hash): env_define, env_get, env_assign.
 *
 * Usa il parser e l'ambiente VERI (src/parser.c, src/env.c): l'unica cosa
 * "parlante" e' l'esecuzione, scritta qui sotto apposta per narrarla.
 *
 * Compilazione (dalla cartella principale):
 *   gcc -std=c11 -I src debug/nura_run.c \
 *       src/parser.c src/ast.c src/lexer.c src/token.c src/env.c -o nura_run
 *   ./nura_run "var n = 5; print n * 2;"
 */
#include "parser.h"
#include "ast.h"
#include "env.h"
#include "pause.h"
#include <stdio.h>
#include <math.h>   /* fmod, per l'operatore % */

/* --- disegna lo stato della tabella hash --- */
static void dump_env(Env *env) {
    printf("      [ambiente]  ");
    if (env->count == 0) { printf("(vuoto)\n"); return; }
    int first = 1;
    for (int i = 0; i < ENV_CAPACITY; i++) {
        for (Entry *e = env->buckets[i]; e != NULL; e = e->next) {
            const char *sep;
            if (first) sep = "";
            else       sep = " , ";
            printf("%sbucket[%d]: %s=%g", sep, i, e->name, e->value);
            first = 0;
        }
    }
    printf("\n");
}

static const char *op_str(TokenType op) {
    switch (op) {
        case TOKEN_PLUS: return "+";  case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";  case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQ: return "==";   case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";    case TOKEN_LE: return "<=";
        case TOKEN_GT: return ">";    case TOKEN_GE: return ">=";
        case TOKEN_AND: return "&&";  case TOKEN_OR: return "||";
        default: return "?";
    }
}

/* --- valutazione narrante: mostra le letture/scritture di variabili --- */
static int depth;
static void ind(void) { printf("      "); for (int i = 0; i < depth; i++) printf("  "); }

static double n_eval(Expr *e, Env *env) {
    switch (e->type) {
        case EXPR_NUMBER:
            return e->as.number.value;   /* foglia numerica: silenziosa */

        case EXPR_VARIABLE: {
            double v = 0;
            int ok = env_get(env, e->as.variable.name, &v);
            ind();
            if (ok) printf("leggo variabile '%s'  ->  env_get = %g\n", e->as.variable.name, v);
            else    printf("leggo variabile '%s'  ->  NON definita!\n", e->as.variable.name);
            return v;
        }

        case EXPR_ASSIGN: {
            double v = n_eval(e->as.assign.value, env);
            int ok = env_assign(env, e->as.assign.name, v);
            const char *esito;
            if (ok) esito = "aggiornata";
            else    esito = "NON definita!";
            ind();
            printf("assegno '%s' = %g  ->  env_assign (%s)\n",
                   e->as.assign.name, v, esito);
            return v;
        }

        case EXPR_UNARY: {
            depth++; double r = n_eval(e->as.unary.right, env); depth--;
            if (e->as.unary.op == TOKEN_BANG) {
                double res; if (r != 0) res = 0; else res = 1;
                ind(); printf("!(%g) = %g\n", r, res);
                return res;
            }
            ind(); printf("-(%g) = %g\n", r, -r);
            return -r;
        }

        case EXPR_BINARY: {
            depth++;
            double l = n_eval(e->as.binary.left, env);
            double r = n_eval(e->as.binary.right, env);
            depth--;
            double res = 0; TokenType op = e->as.binary.op;
            if (op==TOKEN_PLUS) res=l+r; else if (op==TOKEN_MINUS) res=l-r;
            else if (op==TOKEN_STAR) res=l*r; else if (op==TOKEN_SLASH) res=l/r;
            else if (op==TOKEN_PERCENT) res=fmod(l,r);
            else if (op==TOKEN_EQ) res=(l==r); else if (op==TOKEN_NEQ) res=(l!=r);
            else if (op==TOKEN_LT) res=(l<r); else if (op==TOKEN_LE) res=(l<=r);
            else if (op==TOKEN_GT) res=(l>r); else if (op==TOKEN_GE) res=(l>=r);
            ind(); printf("%g %s %g = %g\n", l, op_str(op), r, res);
            return res;
        }
        case EXPR_LOGICAL: {
            const char *s = op_str(e->as.logical.op);
            double l = n_eval(e->as.logical.left, env);
            if (e->as.logical.op == TOKEN_OR && l != 0) {
                ind(); printf("%s: sinistro vero -> corto circuito (destro non valutato) -> 1\n", s);
                return 1;
            }
            if (e->as.logical.op == TOKEN_AND && l == 0) {
                ind(); printf("%s: sinistro falso -> corto circuito (destro non valutato) -> 0\n", s);
                return 0;
            }
            double r = n_eval(e->as.logical.right, env);
            double res; if (r != 0) res = 1; else res = 0;
            ind(); printf("%g %s %g -> %g\n", l, s, r, res);
            return res;
        }
    }
    return 0;
}

/* --- esecuzione narrante di un'istruzione --- */
static void n_execute(Stmt *s, Env *env, int n) {
    depth = 0;
    switch (s->type) {
        case STMT_VAR: {
            printf("\n> istruzione %d:  var %s = ...\n", n, s->as.var.name);
            double v = n_eval(s->as.var.initializer, env);
            env_define(env, s->as.var.name, v);
            printf("      env_define(\"%s\", %g)  ->  la variabile entra nella tabella\n",
                   s->as.var.name, v);
            dump_env(env);
            break;
        }
        case STMT_PRINT: {
            printf("\n> istruzione %d:  print ...\n", n);
            double v = n_eval(s->as.print.expr, env);
            printf("      ===> STAMPA: %g\n", v);
            break;
        }
        case STMT_EXPR: {
            printf("\n> istruzione %d:  espressione ;  (valutata per l'effetto)\n", n);
            n_eval(s->as.expr.expr, env);
            dump_env(env);
            break;
        }

        case STMT_BLOCK: {
            printf("\n> istruzione %d:  blocco { ... }\n", n);
            Program *body = &s->as.block.body;
            for (int i = 0; i < body->count; i++) n_execute(body->statements[i], env, i + 1);
            break;
        }

        case STMT_IF: {
            printf("\n> istruzione %d:  if (...)\n", n);
            double cond = n_eval(s->as.if_stmt.condition, env);
            if (cond != 0) {
                printf("      condizione VERA (%g)  ->  eseguo il ramo 'then'\n", cond);
                n_execute(s->as.if_stmt.then_branch, env, n);
            } else if (s->as.if_stmt.else_branch != NULL) {
                printf("      condizione FALSA (0)  ->  eseguo il ramo 'else'\n");
                n_execute(s->as.if_stmt.else_branch, env, n);
            } else {
                printf("      condizione FALSA (0)  ->  niente da fare\n");
            }
            break;
        }

        case STMT_WHILE: {
            printf("\n> istruzione %d:  while (...)\n", n);
            int giro = 0;
            for (;;) {
                double cond = n_eval(s->as.while_stmt.condition, env);
                if (cond == 0) { printf("      condizione FALSA  ->  esco dal ciclo\n"); break; }
                giro++;
                printf("      --- giro %d (condizione vera: %g) ---\n", giro, cond);
                n_execute(s->as.while_stmt.body, env, n);
            }
            break;
        }
    }
}

int main(int argc, char **argv) {
    const char *source;
    if (argc > 1) source = argv[1];
    else          source = "var n = 5; print n * 2;";
    printf("PROGRAMMA: \"%s\"\n", source);

    /* FASE 1+2: il parser vero produce la lista di istruzioni. */
    Program program;
    int err = 0;
    parse_program(source, &program, &err);
    if (err) { program_free(&program); return 1; }

    printf("\n============================================================\n");
    printf("  Il parser ha prodotto %d istruzioni. Ecco i loro alberi:\n", program.count);
    printf("============================================================\n");
    for (int i = 0; i < program.count; i++) {
        Stmt *s = program.statements[i];
        printf("\nistruzione %d: ", i + 1);
        switch (s->type) {
            case STMT_VAR:   printf("VAR '%s' =\n", s->as.var.name);
                             ast_print_tree(s->as.var.initializer); break;
            case STMT_PRINT: printf("PRINT\n"); ast_print_tree(s->as.print.expr); break;
            case STMT_EXPR:  printf("ESPRESSIONE\n"); ast_print_tree(s->as.expr.expr); break;
        }
    }

    /* FASE 4: esecuzione con l'ambiente (tabella hash). */
    printf("\n============================================================\n");
    printf("  Esecuzione: le istruzioni in ordine, sull'ambiente (hash table)\n");
    printf("============================================================\n");
    Env env;
    env_init(&env);
    for (int i = 0; i < program.count; i++) {
        n_execute(program.statements[i], &env, i + 1);
    }

    printf("\n============================================================\n");
    printf("  Ambiente finale\n");
    printf("============================================================\n");
    dump_env(&env);

    env_free(&env);
    program_free(&program);
    wait_before_closing(argc);
    return 0;
}
