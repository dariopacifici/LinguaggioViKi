#define _POSIX_C_SOURCE 200809L
#include "lexer.h"
#include <ctype.h>

void init_token_list(TokenList* list) {
    list->capacity = 8;
    list->count = 0;
    list->tokens = (Token*)malloc(sizeof(Token) * list->capacity);
}

void append_token(TokenList* list, Token token) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->tokens = (Token*)realloc(list->tokens, sizeof(Token) * list->capacity);
    }
    list->tokens[list->count++] = token;
}

// Legge una stringa che inizia in source[*pos] ('"' oppure '\'').
// Ritorna il contenuto (senza virgolette, escape risolte) o NULL se non terminata.
static char* read_string(const char* source, int* pos, int* line) {
    char quote = source[*pos];
    int i = *pos + 1;
    size_t cap = 16, len = 0;
    char* buf = (char*)malloc(cap);

    while (source[i] != quote) {
        if (source[i] == '\0') {
            free(buf);
            return NULL;
        }

        char ch = source[i++];
        if (ch == '\n') (*line)++;

        if (ch == '\\') {
            switch (source[i]) {
                case 'n':  ch = '\n'; break;
                case 't':  ch = '\t'; break;
                case '\0': free(buf); return NULL;
                default:   ch = source[i]; break;   // \" \' \\ e il resto: carattere letterale
            }
            i++;
        }

        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
        }
        buf[len++] = ch;
    }

    buf[len] = '\0';
    *pos = i + 1;
    return buf;
}

TokenList lex(const char* source) {
    TokenList list;
    init_token_list(&list);

    int i = 0;
    int line = 1;

    while (source[i] != '\0') {
        char c = source[i];

        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }

        if (c == '\n') {
            line++;
            i++;
            continue;
        }

        // Gestione parole chiave ed identificatori
        if (isalpha((unsigned char)c) || c == '_') {
            int start = i;
            while (isalnum((unsigned char)source[i]) || source[i] == '_') i++;

            int len = i - start;
            char* lexeme = (char*)malloc(len + 1);
            strncpy(lexeme, &source[start], len);
            lexeme[len] = '\0';

            TokenType type = TOKEN_IDENTIFIER;
            if (strcmp(lexeme, "let") == 0) type = TOKEN_LET;
            else if (strcmp(lexeme, "print") == 0) type = TOKEN_PRINT;
            else if (strcmp(lexeme, "if") == 0) type = TOKEN_IF;
            else if (strcmp(lexeme, "else") == 0) type = TOKEN_ELSE;
            else if (strcmp(lexeme, "while") == 0) type = TOKEN_WHILE;
            else if (strcmp(lexeme, "fn") == 0) type = TOKEN_FN;
            else if (strcmp(lexeme, "return") == 0) type = TOKEN_RETURN;

            append_token(&list, (Token){type, lexeme, 0.0, line});
            continue;
        }

        // Gestione numeri
        if (isdigit((unsigned char)c)) {
            int start = i;
            while (isdigit((unsigned char)source[i]) || source[i] == '.') i++;

            int len = i - start;
            char* lexeme = (char*)malloc(len + 1);
            strncpy(lexeme, &source[start], len);
            lexeme[len] = '\0';

            double val = atof(lexeme);
            append_token(&list, (Token){TOKEN_NUMBER, lexeme, val, line});
            continue;
        }

        // Simboli operatore e delimitatori
        Token t;
        t.line = line;
        t.value = 0.0;
        t.lexeme = NULL;

        switch (c) {
            case '+': t.type = TOKEN_PLUS; t.lexeme = strdup("+"); i++; break;
            case '-': t.type = TOKEN_MINUS; t.lexeme = strdup("-"); i++; break;
            case '*': t.type = TOKEN_STAR; t.lexeme = strdup("*"); i++; break;
            case '/': t.type = TOKEN_SLASH; t.lexeme = strdup("/"); i++; break;
            case '=':
                if (source[i + 1] == '=') { t.type = TOKEN_EQUAL; t.lexeme = strdup("=="); i += 2; }
                else                      { t.type = TOKEN_ASSIGN; t.lexeme = strdup("="); i++; }
                break;
            case '!':
                if (source[i + 1] == '=') { t.type = TOKEN_NOT_EQUAL; t.lexeme = strdup("!="); i += 2; break; }
                fprintf(stderr, "Errore Lessicale (riga %d): Carattere non riconosciuto '!'\n", line);
                i++;
                continue;
            case '>':
                if (source[i + 1] == '=') { t.type = TOKEN_GREATER_EQUAL; t.lexeme = strdup(">="); i += 2; }
                else                      { t.type = TOKEN_GREATER; t.lexeme = strdup(">"); i++; }
                break;
            case '<':
                if (source[i + 1] == '=') { t.type = TOKEN_LESS_EQUAL; t.lexeme = strdup("<="); i += 2; }
                else                      { t.type = TOKEN_LESS; t.lexeme = strdup("<"); i++; }
                break;
            case ',': t.type = TOKEN_COMMA; t.lexeme = strdup(","); i++; break;
            case '(': t.type = TOKEN_LPAREN; t.lexeme = strdup("("); i++; break;
            case ')': t.type = TOKEN_RPAREN; t.lexeme = strdup(")"); i++; break;
            case '{': t.type = TOKEN_LBRACE; t.lexeme = strdup("{"); i++; break;
            case '}': t.type = TOKEN_RBRACE; t.lexeme = strdup("}"); i++; break;
            case ';': t.type = TOKEN_SEMICOLON; t.lexeme = strdup(";"); i++; break;
            case '"':
            case '\'': {
                char* text = read_string(source, &i, &line);
                if (text == NULL) {
                    fprintf(stderr, "Errore Lessicale (riga %d): stringa non terminata\n", t.line);
                    i = (int)strlen(source);    // porta i a fine sorgente: il while termina
                    continue;
                }
                t.type = TOKEN_STRING;
                t.lexeme = text;
                break;
            }
            default:
                fprintf(stderr, "Errore Lessicale (riga %d): Carattere non riconosciuto '%c'\n", line, c);
                i++;
                continue;
        }
        append_token(&list, t);
    }

    append_token(&list, (Token){TOKEN_EOF, strdup("EOF"), 0.0, line});
    return list;
}

void free_tokens(TokenList* list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->tokens[i].lexeme);
    }
    free(list->tokens);
}
