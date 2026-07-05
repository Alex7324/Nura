#ifndef TOKEN_H
#define TOKEN_H

/*
 * Un "token" e' l'unita' lessicale minima: il pezzetto piu' piccolo di
 * codice che ha un significato. Nel testo "1 + 2" ci sono tre token:
 * il numero 1, l'operatore +, il numero 2.
 *
 * Il lexer trasforma il testo grezzo in un flusso di questi token, cosi'
 * che il parser (Fase 2) non debba piu' ragionare carattere per carattere.
 */

typedef enum {
    /* Letterali */
    TOKEN_NUMBER,      /* 123, 3.14                              */
    TOKEN_STRING,      /* "ciao"                                 */
    TOKEN_IDENTIFIER,  /* nome di variabile o funzione: n, fattoriale */

    /* Parole chiave (identificatori "riservati" dal linguaggio) */
    TOKEN_VAR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_PRINT,
    TOKEN_TRUE,
    TOKEN_FALSE,

    /* Operatori aritmetici */
    TOKEN_PLUS,        /* +  */
    TOKEN_MINUS,       /* -  */
    TOKEN_STAR,        /* *  */
    TOKEN_SLASH,       /* /  */
    TOKEN_PERCENT,     /* %  */

    /* Assegnazione e confronti */
    TOKEN_ASSIGN,      /* =  */
    TOKEN_EQ,          /* == */
    TOKEN_BANG,        /* !  */
    TOKEN_NEQ,         /* != */
    TOKEN_LT,          /* <  */
    TOKEN_LE,          /* <= */
    TOKEN_GT,          /* >  */
    TOKEN_GE,          /* >= */

    /* Operatori logici */
    TOKEN_AND,         /* && */
    TOKEN_OR,          /* || */

    /* Punteggiatura / struttura */
    TOKEN_LPAREN,      /* (  */
    TOKEN_RPAREN,      /* )  */
    TOKEN_LBRACE,      /* {  */
    TOKEN_RBRACE,      /* }  */
    TOKEN_LBRACKET,    /* [  */
    TOKEN_RBRACKET,    /* ]  */
    TOKEN_SEMICOLON,   /* ;  */
    TOKEN_COMMA,       /* ,  */

    /* Token speciali */
    TOKEN_EOF,         /* fine dell'input                        */
    TOKEN_ERROR        /* carattere non riconosciuto             */
} TokenType;

/*
 * Un Token NON contiene una copia del testo. Contiene un puntatore
 * (start) dentro il sorgente originale piu' una lunghezza (length).
 * Cosi' evitiamo di allocare memoria per ogni singolo token.
 *
 * ATTENZIONE: questo significa che il sorgente deve restare vivo in
 * memoria finche' usiamo i token. Nella Fase 1 e' sempre cosi'.
 */
typedef struct {
    TokenType type;
    const char *start;  /* primo carattere del lessema nel sorgente     */
    int length;         /* quanti caratteri e' lungo il lessema         */
    int line;           /* riga del sorgente (utile per gli errori)     */
    double number;      /* valore numerico, valido solo se TOKEN_NUMBER */
} Token;

/* Ritorna il nome testuale del tipo di token (utile per stampare/debug). */
const char *token_type_name(TokenType type);

#endif /* TOKEN_H */
