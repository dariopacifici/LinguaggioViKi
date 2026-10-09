#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "parser.h"

typedef enum { VAL_NUMBER, VAL_STRING, VAL_BOOL } ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        char*  string;  
          int boolean;     // allocata con malloc: il Value ne è proprietario
    } as;
} Value;

typedef struct Symbol {
    char* name;
    Value value;            // era: double value
    struct Symbol* next;
} Symbol;

void  set_variable(const char* name, Value value);   // prende possesso di value
Value get_variable(const char* name);                // ritorna una copia
Value evaluate(ASTNode* node);                       // il chiamante libera il risultato
void  free_value(Value v);
void  free_symbol_table(void);

#endif // INTERPRETER_H