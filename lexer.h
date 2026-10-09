#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definizione di tutti i token gestiti dal Lexer
typedef enum {
    TOKEN_EOF,
    
    // Keywords
    TOKEN_LET,
    TOKEN_PRINT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FN,
    TOKEN_RETURN,

    // Identificatori e Literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    // Operatori Aritmetici e Assegnamento
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_ASSIGN,

    // Confronti
    TOKEN_GREATER,
    TOKEN_LESS,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,

    // Delimitatori
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA
} TokenType;

typedef struct {
    TokenType type;
    char* lexeme;   // Il testo corrispondente (es. "x", "10", "let")
    double value;   // Valore numerico (usato se type == TOKEN_NUMBER)
    int line;       // Riga nel sorgente per reporting errori
} Token;

// Struttura dinamica per contenere la lista dei token generati
typedef struct {
    Token* tokens;
    size_t count;
    size_t capacity;
} TokenList;

// Funzioni pubbliche del Lexer
TokenList lex(const char* source);
void free_tokens(TokenList* list);

#endif // LEXER_H
