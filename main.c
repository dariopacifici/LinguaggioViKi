#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

#define MAX_INPUT_SIZE 1024

// Funzione helper per verificare se un file esiste
bool file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

// Funzione helper per leggere un intero file in memoria
char* read_file(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, sizeof(char), file_size, file);
    buffer[file_size] = '\0';

    fclose(file);
    return buffer;
}

// Esegue una stringa di codice sorgente (da file o da riga di comando)
void execute_source(const char* source) {
    TokenList tokens = lex(source);
    if (tokens.count > 0) {
        ASTNode* root_ast = parse(&tokens);
        if (root_ast) {
            evaluate(root_ast);
            free_ast(root_ast);
        }
    }
    free_tokens(&tokens);
}

// Rimuove spazi vuoti e caratteri 'a capo' a fine riga
void trim_newline(char* str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

int main(int argc, char* argv[]) {
    // Se l'utente passa il file direttamente da riga di comando (es. ./mio_lang test.txt)
    if (argc >= 2) {
        char* source = read_file(argv[1]);
        if (source) {
            execute_source(source);
            free(source);
        } else {
            fprintf(stderr, "Errore: impossibile aprire il file '%s'\n", argv[1]);
        }
        free_symbol_table();
        return 0;
    }

    // Altrimenti avvia la Shell Interattiva ViKi
    printf("======================================\n");
    printf("   Benvenuto nell'Interprete ViKi     \n");
    printf("   Digita il path di un file o codice \n");
    printf("   Digita 'exit' per uscire           \n");
    printf("======================================\n\n");

    char input[MAX_INPUT_SIZE];

    while (true) {
        printf("ViKi:> ");
        fflush(stdout); // Forza l'output immediato del prompt

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }

        trim_newline(input);

        // Se l'input è vuoto, ignora
        if (strlen(input) == 0) {
            continue;
        }

        // Comando per uscire
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Arrivederci!\n");
            break;
        }

        // Se l'input corrisponde a un file esistente, lo legge ed esegue
        if (file_exists(input)) {
            char* source = read_file(input);
            if (source) {
                execute_source(source);
                free(source);
            }
        } else {
            // Altrimenti valuta l'input come codice sorgente ViKi diretto
            execute_source(input);
        }
        printf("\n");
    }

    free_symbol_table();
    return 0;
}