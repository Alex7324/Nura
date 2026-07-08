#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Copia un lessema (start..start+length) in una nuova stringa con '\0'. */
static char *dup_lexeme(const char *start, int length) {
    char *s = malloc((size_t)length + 1);
    if (s == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    memcpy(s, start, (size_t)length);
    s[length] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */
/*  Meccanica di base                                                 */
/* ------------------------------------------------------------------ */

static void error_at(Parser *p, Token token, const char *message) {
    if (p->had_error) return;
    p->had_error = 1;
    fflush(stdout);   /* output del programma prima dell'errore (vedi runtime_error) */
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
static Expr *unary_inner(Parser *p);
static Expr *call(Parser *p);
static Expr *primary(Parser *p);
static Expr *primary_inner(Parser *p);
static Expr *array_literal(Parser *p);

/* Guard-rail contro lo stack overflow. Protegge da DUE forme di espressione
 * troppo profonda, entrambe capaci di far esplodere lo stack del C:
 *
 *  1) ANNIDAMENTO: 5000 parentesi o '[' annidati -> la discesa ricorsiva
 *     (primary/unary) esplode gia' durante il PARSING.
 *  2) CATENE lunghe: 1+1+1+...+1 con migliaia di termini. Il parser le
 *     costruisce in modo iterativo (i while qui sotto), ma producono un albero
 *     profondo che poi esplode altrove: nell'evaluator (ricorsivo) e persino
 *     in ast_free (che libera l'albero in modo ricorsivo).
 *
 * Un unico contatore, p->depth, misura la complessita' dell'espressione
 * corrente: lo incrementano sia i wrapper primary/unary (annidamento) sia i
 * cicli delle catene binarie/logiche (un colpo per operatore). Viene azzerato
 * a ogni istruzione (vedi statement). Oltre il limite: errore controllato.
 * 1000 e' enorme per qualunque codice reale, e ben sotto la soglia di crash. */
#define MAX_PARSE_DEPTH 1000

static int too_deep(Parser *p) {
    if (p->depth >= MAX_PARSE_DEPTH) {
        error_at(p, p->current, "espressione troppo annidata.");
        return 1;
    }
    return 0;
}

/* Traduce le sequenze di escape di una stringa (\n, \t, \r, \", \\) nei byte
 * veri. `src`/`len` sono il testo GREZZO del token (col backslash, cosi' come
 * punta nel sorgente). Ritorna un buffer nuovo (che il chiamante liberera') e
 * ne scrive la lunghezza in *out_len. Il risultato non e' mai piu' lungo
 * dell'originale, perche' ogni escape (2 caratteri) diventa 1 byte. */
static char *process_escapes(Parser *p, const char *src, int len, int *out_len) {
    char *out = malloc((size_t)len + 1);
    if (out == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] != '\\' || i + 1 >= len) {
            out[j++] = src[i];
            continue;
        }
        char e = src[i + 1];
        i++;   /* consumo anche il carattere dopo il backslash */
        if      (e == 'n')  out[j++] = '\n';
        else if (e == 't')  out[j++] = '\t';
        else if (e == 'r')  out[j++] = '\r';
        else if (e == '"')  out[j++] = '"';
        else if (e == '\\') out[j++] = '\\';
        else {
            char msg[64];
            snprintf(msg, sizeof(msg), "sequenza di escape sconosciuta '\\%c'.", e);
            error_at(p, p->current, msg);
            out[j++] = e;   /* fallback: tengo solo il carattere */
        }
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

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

        /* arr[i] = value : il bersaglio e' un'indicizzazione. Riusiamo i suoi
         * due figli (array e indice) e liberiamo solo il "guscio" EXPR_INDEX
         * con free() diretta (NON ast_free, che libererebbe anche i figli). */
        if (left->type == EXPR_INDEX) {
            Expr *arr = left->as.index.array;
            Expr *idx = left->as.index.index;
            free(left);
            return ast_index_set(arr, idx, value);
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
        if (too_deep(p)) break;
        p->depth++;
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
        if (too_deep(p)) break;
        p->depth++;
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
        if (too_deep(p)) break;
        p->depth++;
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
        if (too_deep(p)) break;
        p->depth++;
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
        if (too_deep(p)) break;
        p->depth++;
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
        if (too_deep(p)) break;
        p->depth++;
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        left = ast_binary(op, left, right);
    }
    return left;
}

/* Wrapper con guard-rail: conta la profondita' e la limita. Il lavoro vero e'
 * in unary_inner. Idem per primary/primary_inner piu' sotto. */
static Expr *unary(Parser *p) {
    if (too_deep(p)) return ast_number(0);
    p->depth++;
    Expr *e = unary_inner(p);
    p->depth--;
    return e;
}

static Expr *unary_inner(Parser *p) {
    if (check(p, TOKEN_MINUS) || check(p, TOKEN_BANG)) {
        TokenType op = p->current.type;
        advance(p);
        Expr *right = unary(p);
        return ast_unary(op, right);
    }
    return call(p);
}

/* Legge la lista di argomenti dopo '(' e costruisce il nodo di chiamata.
 * La '(' e' gia' stata consumata. */
static Expr *finish_call(Parser *p, Expr *callee) {
    Expr **args = NULL;
    int count = 0;
    int cap = 0;
    if (!check(p, TOKEN_RPAREN)) {
        do {
            if (count == cap) {
                if (cap < 4) cap = 4;
                else         cap = cap * 2;
                Expr **grown = realloc(args, sizeof(Expr *) * (size_t)cap);
                if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
                args = grown;
            }
            args[count++] = expression(p);
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, "Mi aspettavo ')' dopo gli argomenti.");
    return ast_call(callee, args, count);
}

/* Legge un literal di array dopo '[' e costruisce il nodo EXPR_ARRAY.
 * La '[' e' gia' stata consumata. Stessa meccanica di finish_call, ma con
 * le parentesi quadre e senza un "callee" davanti. */
static Expr *array_literal(Parser *p) {
    Expr **elements = NULL;
    int count = 0;
    int cap = 0;
    if (!check(p, TOKEN_RBRACKET)) {   /* array non vuoto */
        do {
            if (count == cap) {
                if (cap < 4) cap = 4;
                else         cap = cap * 2;
                Expr **grown = realloc(elements, sizeof(Expr *) * (size_t)cap);
                if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
                elements = grown;
            }
            elements[count++] = expression(p);
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RBRACKET, "Mi aspettavo ']' per chiudere l'array.");
    return ast_array(elements, count);
}

/* call -> primary ( "(" argomenti? ")" | "[" expression "]" )*
 * Una cosa "atomica" (primary) seguita da zero o piu' POSTFISSI: chiamate
 * f(...) e indicizzazioni arr[i]. Sono allo stesso livello e si concatenano
 * da sinistra, cosi' funzionano cose come  matrice[i][j]  oppure  f()[0]. */
static Expr *call(Parser *p) {
    Expr *e = primary(p);
    for (;;) {
        if (match(p, TOKEN_LPAREN)) {
            e = finish_call(p, e);
        } else if (match(p, TOKEN_LBRACKET)) {
            Expr *index = expression(p);
            consume(p, TOKEN_RBRACKET, "Mi aspettavo ']' dopo l'indice.");
            e = ast_index(e, index);
        } else {
            break;
        }
    }
    return e;
}

static Expr *primary(Parser *p) {
    if (too_deep(p)) return ast_number(0);
    p->depth++;
    Expr *e = primary_inner(p);
    p->depth--;
    return e;
}

/* primary -> NUMBER | IDENTIFIER | "(" expression ")" | array */
static Expr *primary_inner(Parser *p) {
    if (check(p, TOKEN_NUMBER)) {
        double value = p->current.number;
        advance(p);
        return ast_number(value);
    }

    if (check(p, TOKEN_STRING)) {
        int outlen;
        char *processed = process_escapes(p, p->current.start, p->current.length, &outlen);
        Expr *s = ast_string(processed, outlen);   /* ast_string ne fa la sua copia */
        free(processed);
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

    /* Un '[' apre un literal di array: [1, 2, 3]  (anche vuoto: []). */
    if (match(p, TOKEN_LBRACKET)) {
        return array_literal(p);
    }

    error_at(p, p->current, "Mi aspettavo un numero, un nome, una '(' o un '['.");
    advance(p);
    return ast_number(0);
}

/* ------------------------------------------------------------------ */
/*  Grammatica delle ISTRUZIONI                                       */
/* ------------------------------------------------------------------ */

static Stmt *statement(Parser *p);
static Stmt *var_declaration(Parser *p);
static Stmt *fun_declaration(Parser *p);
static Stmt *print_statement(Parser *p);
static Stmt *return_statement(Parser *p);
static Stmt *expression_statement(Parser *p);
static Stmt *block_statement(Parser *p);
static Stmt *if_statement(Parser *p);
static Stmt *while_statement(Parser *p);
static Stmt *for_statement(Parser *p);
static Stmt *break_statement(Parser *p);
static Stmt *continue_statement(Parser *p);
static Stmt *recur_statement(Parser *p);
static Stmt *statement_inner(Parser *p);

/* Limite all'ANNIDAMENTO delle istruzioni ({ { ... } }, if dentro if, ecc.).
 * Come per le espressioni, la discesa ricorsiva del parser usa lo stack del C:
 * senza questo limite, migliaia di blocchi/if annidati lo farebbero esplodere
 * (crash secco) PRIMA che l'evaluator possa fermarli. Largo per qualunque
 * codice reale (nessuno annida istruzioni 1000 volte). */
#define MAX_STMT_DEPTH 1000

/* Wrapper con guard-rail sulla profondita' di annidamento delle istruzioni.
 * Il lavoro vero e' in statement_inner. */
static Stmt *statement(Parser *p) {
    if (p->stmt_depth >= MAX_STMT_DEPTH) {
        error_at(p, p->current, "troppe istruzioni annidate.");
        return stmt_expr(ast_number(0));   /* segnaposto: had_error ferma il parsing */
    }
    p->stmt_depth++;
    Stmt *s = statement_inner(p);
    p->stmt_depth--;
    return s;
}

/* statement -> funDecl | varDecl | printStmt | ifStmt | whileStmt | forStmt |
 *              returnStmt | block | exprStmt */
static Stmt *statement_inner(Parser *p) {
    /* Ogni istruzione riparte con un "budget" di profondita' fresco: il
     * contatore p->depth serve a limitare la complessita' di UNA espressione
     * (vedi too_deep e le catene binarie), non a sommarsi tra istruzioni. */
    p->depth = 0;
    if (match(p, TOKEN_FUN))    return fun_declaration(p);
    if (match(p, TOKEN_VAR))    return var_declaration(p);
    if (match(p, TOKEN_PRINT))  return print_statement(p);
    if (match(p, TOKEN_IF))     return if_statement(p);
    if (match(p, TOKEN_WHILE))  return while_statement(p);
    if (match(p, TOKEN_FOR))    return for_statement(p);
    if (match(p, TOKEN_BREAK))    return break_statement(p);
    if (match(p, TOKEN_CONTINUE)) return continue_statement(p);
    if (match(p, TOKEN_RETURN)) return return_statement(p);
    if (match(p, TOKEN_RECUR))  return recur_statement(p);
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

    p->loop_depth++;   /* dentro il corpo, break/continue sono leciti */
    Stmt *body = statement(p);
    p->loop_depth--;
    return stmt_while(condition, body);
}

/* forStmt -> "for" "(" ( varDecl | exprStmt | ";" )
 *                       expression? ";"
 *                       expression? ")" statement            ('for' gia' consumato)
 *
 * Il 'for' e' un costrutto a se' (STMT_FOR). All'inizio era zucchero sintattico
 * tradotto in un while, ma questo rendeva scorretto il 'continue' (che deve
 * saltare all'incremento): per gestirlo bene serve un vero nodo for. Ogni
 * clausola (iniz; cond; incr) e' facoltativa. */
static Stmt *for_statement(Parser *p) {
    consume(p, TOKEN_LPAREN, "Mi aspettavo '(' dopo 'for'.");

    /* 1) Inizializzatore: una dichiarazione 'var', un'espressione, o niente.
     *    Ognuna di queste consuma anche il proprio ';'. */
    Stmt *initializer;
    if (match(p, TOKEN_SEMICOLON)) {
        initializer = NULL;                    /* for (; ...; ...) */
    } else if (match(p, TOKEN_VAR)) {
        initializer = var_declaration(p);      /* var i = 0;       */
    } else {
        initializer = expression_statement(p); /* i = 0;           */
    }

    /* 2) Condizione: se manca, il ciclo e' "sempre vero". */
    Expr *condition = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        condition = expression(p);
    }
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo la condizione del for.");

    /* 3) Incremento: un'espressione da eseguire dopo ogni giro (o niente). */
    Expr *increment = NULL;
    if (!check(p, TOKEN_RPAREN)) {
        increment = expression(p);
    }
    consume(p, TOKEN_RPAREN, "Mi aspettavo ')' dopo le clausole del for.");

    /* 4) Corpo del ciclo. Dentro il corpo, break/continue sono leciti. */
    p->loop_depth++;
    Stmt *body = statement(p);
    p->loop_depth--;

    return stmt_for(initializer, condition, increment, body);
}

/* breakStmt / continueStmt -> ("break" | "continue") ";"
 * Ammessi solo dentro un ciclo (lo verifichiamo con loop_depth, gia' in parsing). */
static Stmt *break_statement(Parser *p) {
    if (p->loop_depth == 0)
        error_at(p, p->previous, "'break' fuori da un ciclo.");
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo 'break'.");
    return stmt_break();
}

static Stmt *continue_statement(Parser *p) {
    if (p->loop_depth == 0)
        error_at(p, p->previous, "'continue' fuori da un ciclo.");
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo 'continue'.");
    return stmt_continue();
}

/* funDecl -> "fun" IDENTIFIER "(" ( IDENTIFIER ( "," IDENTIFIER )* )? ")" block
 * ('fun' gia' consumato). */
static Stmt *fun_declaration(Parser *p) {
    Token name = p->current;
    consume(p, TOKEN_IDENTIFIER, "Mi aspettavo il nome della funzione dopo 'fun'.");
    consume(p, TOKEN_LPAREN, "Mi aspettavo '(' dopo il nome della funzione.");

    char **params = NULL;
    int count = 0;
    int cap = 0;
    if (!check(p, TOKEN_RPAREN)) {
        do {
            Token pname = p->current;
            consume(p, TOKEN_IDENTIFIER, "Mi aspettavo il nome di un parametro.");
            if (count == cap) {
                if (cap < 4) cap = 4;
                else         cap = cap * 2;
                char **grown = realloc(params, sizeof(char *) * (size_t)cap);
                if (grown == NULL) { fprintf(stderr, "Memoria esaurita.\n"); exit(1); }
                params = grown;
            }
            params[count++] = dup_lexeme(pname.start, pname.length);
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, "Mi aspettavo ')' dopo i parametri.");
    consume(p, TOKEN_LBRACE, "Mi aspettavo '{' per il corpo della funzione.");

    /* Il corpo e' un "nuovo mondo" per break/continue: un ciclo esterno non li
     * rende leciti qui dentro. Azzero e ripristino loop_depth. Al contrario
     * 'recur' e' lecito PROPRIO dentro una funzione: alzo fun_depth. */
    int saved_loop = p->loop_depth;
    p->loop_depth = 0;
    p->fun_depth++;
    Stmt *body = block_statement(p);   /* la '{' e' gia' stata consumata */
    p->fun_depth--;
    p->loop_depth = saved_loop;
    return stmt_fun(name.start, name.length, params, count, body);
}

/* returnStmt -> "return" expression? ";"   ('return' gia' consumato) */
static Stmt *return_statement(Parser *p) {
    Expr *value = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        value = expression(p);   /* return con un valore */
    }
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo il valore di 'return'.");
    return stmt_return(value);
}

/* recurStmt -> "recur" call ";"   ('recur' gia' consumato)
 *
 * 'recur' e' un'ISTRUZIONE (come return), non un'espressione: cosi' la
 * "posizione di coda" e' garantita dalla sintassi stessa (non puoi scrivere
 * 'recur f(x) + 1', perche' non produce un valore). Vincoli:
 *   - lecito solo dentro una funzione (fun_depth > 0);
 *   - il valore DEVE essere una chiamata di funzione (EXPR_CALL).
 * L'ottimizzazione della chiamata in coda (trampolino) la fa l'evaluator. */
static Stmt *recur_statement(Parser *p) {
    if (p->fun_depth == 0)
        error_at(p, p->previous, "'recur' fuori da una funzione.");
    Expr *call = expression(p);
    if (call != NULL && call->type != EXPR_CALL)
        error_at(p, p->previous, "'recur' vuole una chiamata di funzione, es. 'recur f(x)'.");
    consume(p, TOKEN_SEMICOLON, "Mi aspettavo ';' dopo la chiamata di 'recur'.");
    return stmt_recur(call);
}

/* ------------------------------------------------------------------ */
/*  Punti di ingresso pubblici                                        */
/* ------------------------------------------------------------------ */

Expr *parse_expression_source(const char *source, int *had_error) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;
    parser.depth = 0;
    parser.loop_depth = 0;
    parser.stmt_depth = 0;
    parser.fun_depth = 0;
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
    parser.depth = 0;
    parser.loop_depth = 0;
    parser.stmt_depth = 0;
    parser.fun_depth = 0;
    advance(&parser);            /* primer */

    program_init(program);
    while (!check(&parser, TOKEN_EOF)) {
        Stmt *s = statement(&parser);
        program_add(program, s);
        if (parser.had_error) break;   /* ci fermiamo al primo errore */
    }

    *had_error = parser.had_error;
}
