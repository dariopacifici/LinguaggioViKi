#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

// Tipi di nodi che formano l'Albero Sintattico Astratto (AST)
typedef enum {
    NODE_NUMBER,
    NODE_STRING,
    NODE_VARIABLE,
    NODE_BINARY_OP,
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_IF,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_FUNC_DECL,
    NODE_CALL,
    NODE_RETURN
} NodeType;

struct ASTNode; // Forward declaration

// Nodo per operazioni binarie (es. a + b, x > 5)
typedef struct {
    TokenType op;
    struct ASTNode* left;
    struct ASTNode* right;
} BinaryOpNode;

// Nodo per dichiarazione di variabili (es. let x = 10)
typedef struct {
    char* var_name;
    struct ASTNode* initializer;
} VarDeclNode;

// Nodo per assegnamento (es. x = 20)
typedef struct {
    char* var_name;
    struct ASTNode* value;
} AssignNode;

// Nodo per blocchi di codice racchiusi tra { }
typedef struct {
    struct ASTNode** statements;
    size_t count;
} BlockNode;

// Nodo per if / else (else_branch può essere NULL)
typedef struct {
    struct ASTNode* condition;
    struct ASTNode* then_branch;
    struct ASTNode* else_branch;
} IfNode;

// Nodo per while (es. while (x < 10) { ... })
typedef struct {
    struct ASTNode* condition;
    struct ASTNode* body;
} WhileNode;

// Nodo per dichiarazione di funzione (es. fn somma(a, b) { ... })
typedef struct {
    char* name;
    char** params;
    size_t param_count;
    struct ASTNode* body;
    int registered;          // 1 = il nodo è posseduto dalla tabella delle funzioni
} FuncDeclNode;

// Nodo per chiamata di funzione (es. somma(2, 3))
typedef struct {
    char* name;
    struct ASTNode** args;
    size_t argc;
} CallNode;

// Struttura unificata dell'ASTNode
typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        double num_value;        // Per NODE_NUMBER
        char* str_value;         // Per NODE_STRING
        char* var_name;          // Per NODE_VARIABLE
        BinaryOpNode binary;     // Per NODE_BINARY_OP
        VarDeclNode var_decl;    // Per NODE_VAR_DECL
        AssignNode assign;       // Per NODE_ASSIGN
        BlockNode block;         // Per NODE_BLOCK
        IfNode if_stmt;          // Per NODE_IF
        WhileNode while_stmt;    // Per NODE_WHILE
        FuncDeclNode func;       // Per NODE_FUNC_DECL
        CallNode call;           // Per NODE_CALL
        struct ASTNode* expr;    // Per NODE_PRINT e NODE_RETURN (può essere NULL)
    };
} ASTNode;

// Funzioni pubbliche del Parser
ASTNode* parse(TokenList* tokens);
void free_ast(ASTNode* node);
void free_ast_owned(ASTNode* node);   // libera anche un NODE_FUNC_DECL registrato

// Costruttori Helper per la creazione dei nodi
ASTNode* create_number_node(double val);
ASTNode* create_variable_node(const char* name);
ASTNode* create_binary_node(TokenType op, ASTNode* left, ASTNode* right);
ASTNode* create_var_decl_node(const char* name, ASTNode* init);
ASTNode* create_assign_node(const char* name, ASTNode* value);
ASTNode* create_print_node(ASTNode* expr);
ASTNode* create_string_node(const char* text);
ASTNode* create_if_node(ASTNode* cond, ASTNode* then_branch, ASTNode* else_branch);
ASTNode* create_while_node(ASTNode* cond, ASTNode* body);
ASTNode* create_return_node(ASTNode* expr);
ASTNode* create_call_node(const char* name, ASTNode** args, size_t argc);
ASTNode* create_func_decl_node(const char* name, char** params, size_t param_count, ASTNode* body);

#endif // PARSER_H
