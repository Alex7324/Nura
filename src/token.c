#include "token.h"

/*
 * Traduce un TokenType nel suo nome leggibile.
 * Serve solo per stampare i token in modo comprensibile durante il debug;
 * non fa parte della logica del lexer vera e propria.
 *
 * Nota: NON mettiamo un "default:" nello switch. Cosi', se un giorno
 * aggiungi un nuovo TokenType e ti dimentichi di aggiornarlo qui, gcc
 * con -Wall ti avvisa che un caso non e' gestito. E' un promemoria gratis.
 */
const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_NUMBER:     return "NUMBER";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_VAR:        return "VAR";
        case TOKEN_FUN:        return "FUN";
        case TOKEN_IF:         return "IF";
        case TOKEN_ELSE:       return "ELSE";
        case TOKEN_WHILE:      return "WHILE";
        case TOKEN_RETURN:     return "RETURN";
        case TOKEN_PRINT:      return "PRINT";
        case TOKEN_PLUS:       return "PLUS";
        case TOKEN_MINUS:      return "MINUS";
        case TOKEN_STAR:       return "STAR";
        case TOKEN_SLASH:      return "SLASH";
        case TOKEN_ASSIGN:     return "ASSIGN";
        case TOKEN_EQ:         return "EQ";
        case TOKEN_BANG:       return "BANG";
        case TOKEN_NEQ:        return "NEQ";
        case TOKEN_LT:         return "LT";
        case TOKEN_LE:         return "LE";
        case TOKEN_GT:         return "GT";
        case TOKEN_GE:         return "GE";
        case TOKEN_LPAREN:     return "LPAREN";
        case TOKEN_RPAREN:     return "RPAREN";
        case TOKEN_LBRACE:     return "LBRACE";
        case TOKEN_RBRACE:     return "RBRACE";
        case TOKEN_SEMICOLON:  return "SEMICOLON";
        case TOKEN_COMMA:      return "COMMA";
        case TOKEN_EOF:        return "EOF";
        case TOKEN_ERROR:      return "ERROR";
    }
    return "???"; /* irraggiungibile, ma tiene tranquillo il compilatore */
}
