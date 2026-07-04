/* nura_run — narratore dell'ESECUZIONE di un programma.
 *
 * Mostra un PROGRAMMA intero: la lista di istruzioni prodotta dal parser, e
 * poi l'esecuzione passo per passo, con le operazioni sull'AMBIENTE (la
 * tabella hash): env_define, env_get, env_assign. I valori possono essere
 * numeri, booleani o stringhe (Value).
 *
 * Usa il parser e l'ambiente VERI (src/parser.c, src/env.c, src/value.c):
 * l'unica cosa "parlante" e' l'esecuzione, scritta qui apposta per narrarla.
 *
 * Compilazione (dalla cartella principale):
 *   gcc -std=gnu11 -I src debug/nura_run.c \
 *       src/parser.c src/ast.c src/lexer.c src/token.c src/env.c src/value.c \
 *       -o nura_run -lm
 */
#include "parser.h"
#include "ast.h"
#include "env.h"
#include "pause.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
            printf("%sbucket[%d]: %s=", sep, i, e->name);
            value_print(e->value);
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

/* Stato per le funzioni: 'globals' e' lo scope che ogni chiamata racchiude;
 * 'returning'/'ret_val' sono il meccanismo del return (come nell'interprete). */
static Env *globals;
static int returning;
static Value ret_val;

static void n_execute(Stmt *s, Env *env, int n);   /* dichiarazione anticipata */

static Value n_eval(Expr *e, Env *env) {
    switch (e->type) {
        case EXPR_NUMBER: return value_number(e->as.number.value);
        case EXPR_BOOL:   return value_bool(e->as.boolean.value);
        case EXPR_STRING: return value_string(e->as.string.value);

        case EXPR_VARIABLE: {
            Value v;
            int ok = env_get(env, e->as.variable.name, &v);
            ind();
            if (ok) {
                printf("leggo variabile '%s'  ->  env_get = ", e->as.variable.name);
                value_print(v); printf("\n");
            } else {
                printf("leggo variabile '%s'  ->  NON definita!\n", e->as.variable.name);
                v = value_number(0);
            }
            return v;
        }

        case EXPR_ASSIGN: {
            Value v = n_eval(e->as.assign.value, env);
            int ok = env_assign(env, e->as.assign.name, v);
            const char *esito;
            if (ok) esito = "aggiornata";
            else    esito = "NON definita!";
            ind();
            printf("assegno '%s' = ", e->as.assign.name);
            value_print(v);
            printf("  ->  env_assign (%s)\n", esito);
            return v;
        }

        case EXPR_UNARY: {
            depth++; Value r = n_eval(e->as.unary.right, env); depth--;
            if (e->as.unary.op == TOKEN_BANG) {
                Value res = value_bool(!value_is_truthy(r));
                ind(); printf("!"); value_print(r); printf(" = "); value_print(res); printf("\n");
                return res;
            }
            Value res = value_number(-r.as.number);
            ind(); printf("-("); value_print(r); printf(") = "); value_print(res); printf("\n");
            return res;
        }

        case EXPR_BINARY: {
            depth++;
            Value l = n_eval(e->as.binary.left, env);
            Value r = n_eval(e->as.binary.right, env);
            depth--;
            TokenType op = e->as.binary.op;
            Value res;
            if (op == TOKEN_EQ)       res = value_bool(values_equal(l, r));
            else if (op == TOKEN_NEQ) res = value_bool(!values_equal(l, r));
            else if (op == TOKEN_PLUS && l.type == VAL_STRING && r.type == VAL_STRING) {
                size_t la = strlen(l.as.string), lb = strlen(r.as.string);
                char *s = malloc(la + lb + 1);
                memcpy(s, l.as.string, la);
                memcpy(s + la, r.as.string, lb + 1);
                res = value_string(s);   /* piccolo leak: ok per un tool di debug */
            } else {
                double a = l.as.number, b = r.as.number;
                if (op==TOKEN_PLUS) res=value_number(a+b);
                else if (op==TOKEN_MINUS) res=value_number(a-b);
                else if (op==TOKEN_STAR) res=value_number(a*b);
                else if (op==TOKEN_SLASH) res=value_number(a/b);
                else if (op==TOKEN_PERCENT) res=value_number(fmod(a,b));
                else if (op==TOKEN_LT) res=value_bool(a<b);
                else if (op==TOKEN_LE) res=value_bool(a<=b);
                else if (op==TOKEN_GT) res=value_bool(a>b);
                else if (op==TOKEN_GE) res=value_bool(a>=b);
                else res=value_number(0);
            }
            ind();
            value_print(l); printf(" %s ", op_str(op)); value_print(r);
            printf(" = "); value_print(res); printf("\n");
            return res;
        }

        case EXPR_LOGICAL: {
            const char *s = op_str(e->as.logical.op);
            Value l = n_eval(e->as.logical.left, env);
            if (e->as.logical.op == TOKEN_OR && value_is_truthy(l)) {
                ind(); printf("%s: sinistro vero -> corto circuito -> true\n", s);
                return value_bool(1);
            }
            if (e->as.logical.op == TOKEN_AND && !value_is_truthy(l)) {
                ind(); printf("%s: sinistro falso -> corto circuito -> false\n", s);
                return value_bool(0);
            }
            Value r = n_eval(e->as.logical.right, env);
            Value res = value_bool(value_is_truthy(r));
            ind();
            value_print(l); printf(" %s ", s); value_print(r);
            printf(" -> "); value_print(res); printf("\n");
            return res;
        }

        case EXPR_CALL: {
            Value callee = n_eval(e->as.call.callee, env);
            if (callee.type != VAL_FUNCTION) {
                ind(); printf("ERRORE: si possono chiamare solo le funzioni\n");
                return value_number(0);
            }
            Stmt *decl = callee.as.function;
            ind(); printf(">>> CHIAMATA a '%s'  (creo un nuovo scope)\n", decl->as.function.name);

            /* nuovo scope, racchiude il globale (come nell'interprete vero) */
            Env *call_env = malloc(sizeof(Env));   /* non liberato: leak ok per il tool */
            env_init(call_env);
            call_env->enclosing = globals;

            for (int i = 0; i < decl->as.function.param_count && i < e->as.call.arg_count; i++) {
                depth++;
                Value arg = n_eval(e->as.call.args[i], env);
                depth--;
                env_define(call_env, decl->as.function.params[i], arg);
                ind(); printf("    parametro %s = ", decl->as.function.params[i]);
                value_print(arg); printf("\n");
            }

            depth++;
            n_execute(decl->as.function.body, call_env, 1);
            depth--;

            Value result;
            if (returning) { result = ret_val; returning = 0; }
            else           result = value_number(0);

            ind(); printf("<<< '%s' ritorna ", decl->as.function.name);
            value_print(result); printf("\n");
            return result;
        }
    }
    return value_number(0);
}

/* --- esecuzione narrante di un'istruzione --- */
static void n_execute(Stmt *s, Env *env, int n) {
    depth = 0;
    switch (s->type) {
        case STMT_VAR: {
            printf("\n> istruzione %d:  var %s = ...\n", n, s->as.var.name);
            Value v = n_eval(s->as.var.initializer, env);
            env_define(env, s->as.var.name, v);
            printf("      env_define(\"%s\", ", s->as.var.name);
            value_print(v);
            printf(")  ->  la variabile entra nella tabella\n");
            dump_env(env);
            break;
        }
        case STMT_PRINT: {
            printf("\n> istruzione %d:  print ...\n", n);
            Value v = n_eval(s->as.print.expr, env);
            printf("      ===> STAMPA: ");
            value_print(v);
            printf("\n");
            break;
        }
        case STMT_EXPR: {
            printf("\n> istruzione %d:  espressione ;  (valutata per l'effetto)\n", n);
            n_eval(s->as.expr.expr, env);
            dump_env(env);
            break;
        }
        case STMT_BLOCK: {
            printf("\n> istruzione %d:  blocco { ... }  (nuovo scope)\n", n);
            Env *block_env = malloc(sizeof(Env));   /* non liberato: leak ok per il tool */
            env_init(block_env);
            block_env->enclosing = env;
            Program *body = &s->as.block.body;
            for (int i = 0; i < body->count; i++) {
                n_execute(body->statements[i], block_env, i + 1);
                if (returning) break;
            }
            break;
        }
        case STMT_IF: {
            printf("\n> istruzione %d:  if (...)\n", n);
            Value cond = n_eval(s->as.if_stmt.condition, env);
            if (value_is_truthy(cond)) {
                printf("      condizione VERA  ->  eseguo il ramo 'then'\n");
                n_execute(s->as.if_stmt.then_branch, env, n);
            } else if (s->as.if_stmt.else_branch != NULL) {
                printf("      condizione FALSA  ->  eseguo il ramo 'else'\n");
                n_execute(s->as.if_stmt.else_branch, env, n);
            } else {
                printf("      condizione FALSA  ->  niente da fare\n");
            }
            break;
        }
        case STMT_WHILE: {
            printf("\n> istruzione %d:  while (...)\n", n);
            int giro = 0;
            for (;;) {
                Value cond = n_eval(s->as.while_stmt.condition, env);
                if (!value_is_truthy(cond)) { printf("      condizione FALSA  ->  esco dal ciclo\n"); break; }
                giro++;
                printf("      --- giro %d (condizione vera) ---\n", giro);
                n_execute(s->as.while_stmt.body, env, n);
                if (returning) break;
            }
            break;
        }

        case STMT_FUN: {
            printf("\n> istruzione %d:  fun %s(...)  (definizione)\n", n, s->as.function.name);
            env_define(env, s->as.function.name, value_function(s));
            printf("      env_define(\"%s\", <funzione>)  ->  entra nella tabella\n",
                   s->as.function.name);
            dump_env(env);
            break;
        }

        case STMT_RETURN: {
            printf("\n> istruzione %d:  return ...\n", n);
            Value v;
            if (s->as.ret.value != NULL) v = n_eval(s->as.ret.value, env);
            else                         v = value_number(0);
            ret_val = v;
            returning = 1;
            ind(); printf("return -> "); value_print(v);
            printf("   (risalgo fuori dalla funzione)\n");
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

    /* FASE 4-6: esecuzione con l'ambiente (tabella hash). */
    printf("\n============================================================\n");
    printf("  Esecuzione: le istruzioni in ordine, sull'ambiente (hash table)\n");
    printf("============================================================\n");
    Env env;
    env_init(&env);
    globals = &env;
    returning = 0;
    for (int i = 0; i < program.count; i++) {
        n_execute(program.statements[i], &env, i + 1);
        if (returning) break;
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
