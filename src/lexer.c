#include "lexer.h"

#include <ctype.h>   /* isdigit                 */
#include <stdlib.h>  /* strtod                  */
#include <string.h>  /* memcmp, strlen          */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

/* ------------------------------------------------------------------ */
/*  Funzioni di aiuto: sono i "mattoncini" con cui muoviamo le dita.  */
/*  Sono static perche' servono solo qui dentro.                      */
/* ------------------------------------------------------------------ */

/* Siamo arrivati alla fine? Il sorgente e' una stringa C: finisce con '\0'. */
static int is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

/* Ritorna il carattere corrente e POI avanza di uno.
 * (post-incremento: prima si legge *current, poi current++.) */
static char advance(Lexer *lexer) {
    return *lexer->current++;
}

/* Sbircia il carattere corrente SENZA avanzare. */
static char peek(Lexer *lexer) {
    return *lexer->current;
}

/* Sbircia il carattere DOPO quello corrente, senza avanzare.
 * Se siamo a fine input non leggiamo oltre il '\0'. */
static char peek_next(Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

/* Avanza SOLO se il carattere corrente e' quello che ci aspettiamo.
 * Ritorna 1 se ha "consumato" il carattere, 0 altrimenti.
 * E' il trucco per distinguere '=' da '==', '<' da '<=', ecc. */
static int match(Lexer *lexer, char expected) {
    if (is_at_end(lexer)) return 0;
    if (*lexer->current != expected) return 0;
    lexer->current++;
    return 1;
}

/* Costruisce un token del tipo `type` usando l'intervallo start..current. */
static Token make_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.number = 0;
    return token;
}

/* Token di errore: qui `start` punta a un messaggio, non al sorgente. */
static Token error_token(Lexer *lexer, const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.number = 0;
    return token;
}

/* ------------------------------------------------------------------ */
/*  Spazi bianchi e commenti: sono "rumore" che non produce token.    */
/* ------------------------------------------------------------------ */

static void skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;   /* nuova riga: aggiorniamo il contatore */
                advance(lexer);
                break;
            case '/':
                /* "//" inizia un commento fino a fine riga.
                 * Ma un solo '/' e' l'operatore divisione: NON lo tocchiamo. */
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) advance(lexer);
                } else {
                    return;
                }
                break;
            default:
                return;  /* carattere "vero": lo spazio bianco e' finito */
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Numeri                                                            */
/* ------------------------------------------------------------------ */

static Token number(Lexer *lexer) {
    while (isdigit((unsigned char)peek(lexer))) advance(lexer);

    /* Parte decimale opzionale: un '.' seguito da altre cifre.
     * Controlliamo peek_next cosi' "3." senza cifre dopo non ci inganna. */
    if (peek(lexer) == '.' && isdigit((unsigned char)peek_next(lexer))) {
        advance(lexer);  /* consuma il '.' */
        while (isdigit((unsigned char)peek(lexer))) advance(lexer);
    }

    Token token = make_token(lexer, TOKEN_NUMBER);
    /* strtod converte il testo in double. Parte da start e si ferma da solo
     * al primo carattere non numerico: perfetto per noi. */
    token.number = strtod(lexer->start, NULL);
    return token;
}

/* ------------------------------------------------------------------ */
/*  Identificatori e parole chiave                                    */
/* ------------------------------------------------------------------ */

/* Un identificatore inizia con una lettera o '_'. */
static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

/* Abbiamo appena letto una "parola". E' una parola chiave o un nome libero?
 * Confrontiamo il lessema (start..current) con la lista delle keyword. */
static TokenType identifier_type(Lexer *lexer) {
    int length = (int)(lexer->current - lexer->start);
    const char *text = lexer->start;

    static const struct {
        const char *word;
        TokenType type;
    } keywords[] = {
        { "var",    TOKEN_VAR    },
        { "fun",    TOKEN_FUN    },
        { "if",     TOKEN_IF     },
        { "else",   TOKEN_ELSE   },
        { "while",  TOKEN_WHILE  },
        { "return", TOKEN_RETURN },
        { "print",  TOKEN_PRINT  },
    };
    int n = (int)(sizeof(keywords) / sizeof(keywords[0]));

    for (int i = 0; i < n; i++) {
        /* Stessa lunghezza E stessi caratteri => e' quella parola chiave.
         * Usiamo memcmp (non strcmp) perche' il lessema nel sorgente NON
         * finisce con '\0': e' delimitato solo da start..current. */
        if ((int)strlen(keywords[i].word) == length &&
            memcmp(text, keywords[i].word, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;  /* nessuna corrispondenza: e' un nome libero */
}

static Token identifier(Lexer *lexer) {
    /* Dopo la prima lettera puo' esserci qualunque mix di lettere e cifre:
     * es. contatore1, x2, _tmp. */
    while (is_alpha(peek(lexer)) || isdigit((unsigned char)peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, identifier_type(lexer));
}

/* ------------------------------------------------------------------ */
/*  La funzione principale: produce UN token per chiamata.            */
/* ------------------------------------------------------------------ */

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace(lexer);

    /* Il nuovo token comincia da qui: allineiamo start su current. */
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOF);

    char c = advance(lexer);  /* consumiamo il primo carattere del token */

    /* Se inizia con lettera/underscore o cifra, deleghiamo alle funzioni
     * apposite, che continueranno a leggere il resto della parola/numero. */
    if (is_alpha(c)) return identifier(lexer);
    if (isdigit((unsigned char)c)) return number(lexer);

    switch (c) {
        /* Simboli singoli */
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/': return make_token(lexer, TOKEN_SLASH);

        /* Simboli che possono avere un '=' subito dopo.
         * match() decide fra la versione a 1 e a 2 caratteri. */
        case '=': return make_token(lexer, match(lexer, '=') ? TOKEN_EQ  : TOKEN_ASSIGN);
        case '!': return make_token(lexer, match(lexer, '=') ? TOKEN_NEQ : TOKEN_BANG);
        case '<': return make_token(lexer, match(lexer, '=') ? TOKEN_LE  : TOKEN_LT);
        case '>': return make_token(lexer, match(lexer, '=') ? TOKEN_GE  : TOKEN_GT);
    }

    return error_token(lexer, "Carattere non riconosciuto.");
}
