#define _POSIX_C_SOURCE 200809L
#include "parser.h"

static size_t current_token = 0;
static int function_depth = 0;   // > 0 mentre si legge il corpo di una funzione

static Token peek(TokenList* tokens) {
    return tokens->tokens[current_token];
}

// Guarda 'offset' token più avanti senza uscire dalla lista (si ferma su EOF)
static Token peek_at(TokenList* tokens, size_t offset) {
    size_t idx = current_token + offset;
    if (idx >= tokens->count) idx = tokens->count - 1;
    return tokens->tokens[idx];
}

static Token advance(TokenList* tokens) {
    Token t = tokens->tokens[current_token];
    if (t.type != TOKEN_EOF) current_token++;
    return t;
}

static int match(TokenList* tokens, TokenType type) {
    if (peek(tokens).type == type) {
        advance(tokens);
        return 1;
    }
    return 0;
}

// Costruttori Helper
static ASTNode* new_node(NodeType type) {
    ASTNode* n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = type;
    return n;
}

ASTNode* create_number_node(double val) {
    ASTNode* n = new_node(NODE_NUMBER);
    n->num_value = val;
    return n;
}

ASTNode* create_string_node(const char* text) {
    ASTNode* n = new_node(NODE_STRING);
    n->str_value = strdup(text);
    return n;
}

ASTNode* create_variable_node(const char* name) {
    ASTNode* n = new_node(NODE_VARIABLE);
    n->var_name = strdup(name);
    return n;
}

ASTNode* create_binary_node(TokenType op, ASTNode* left, ASTNode* right) {
    ASTNode* n = new_node(NODE_BINARY_OP);
    n->binary.op = op;
    n->binary.left = left;
    n->binary.right = right;
    return n;
}

ASTNode* create_var_decl_node(const char* name, ASTNode* init) {
    ASTNode* n = new_node(NODE_VAR_DECL);
    n->var_decl.var_name = strdup(name);
    n->var_decl.initializer = init;
    return n;
}

ASTNode* create_assign_node(const char* name, ASTNode* value) {
    ASTNode* n = new_node(NODE_ASSIGN);
    n->assign.var_name = strdup(name);
    n->assign.value = value;
    return n;
}

ASTNode* create_print_node(ASTNode* expr) {
    ASTNode* n = new_node(NODE_PRINT);
    n->expr = expr;
    return n;
}

ASTNode* create_if_node(ASTNode* cond, ASTNode* then_branch, ASTNode* else_branch) {
    ASTNode* n = new_node(NODE_IF);
    n->if_stmt.condition = cond;
    n->if_stmt.then_branch = then_branch;
    n->if_stmt.else_branch = else_branch;
    return n;
}

ASTNode* create_while_node(ASTNode* cond, ASTNode* body) {
    ASTNode* n = new_node(NODE_WHILE);
    n->while_stmt.condition = cond;
    n->while_stmt.body = body;
    return n;
}

ASTNode* create_return_node(ASTNode* expr) {
    ASTNode* n = new_node(NODE_RETURN);
    n->expr = expr;
    return n;
}

ASTNode* create_call_node(const char* name, ASTNode** args, size_t argc) {
    ASTNode* n = new_node(NODE_CALL);
    n->call.name = strdup(name);
    n->call.args = args;       // prende possesso dell'array
    n->call.argc = argc;
    return n;
}

ASTNode* create_func_decl_node(const char* name, char** params, size_t param_count, ASTNode* body) {
    ASTNode* n = new_node(NODE_FUNC_DECL);
    n->func.name = strdup(name);
    n->func.params = params;   // prende possesso dell'array e delle stringhe
    n->func.param_count = param_count;
    n->func.body = body;
    n->func.registered = 0;
    return n;
}

// Prototipi della grammatica ricorsiva
static ASTNode* expression(TokenList* tokens);
static ASTNode* statement(TokenList* tokens);
static ASTNode* parse_statements(TokenList* tokens, int in_braces);

// Legge gli argomenti di una chiamata; la '(' è già stata consumata
static ASTNode* parse_call(TokenList* tokens, const char* name) {
    size_t cap = 4, argc = 0;
    ASTNode** args = (ASTNode**)malloc(sizeof(ASTNode*) * cap);

    if (peek(tokens).type != TOKEN_RPAREN) {
        do {
            ASTNode* arg = expression(tokens);
            if (argc >= cap) {
                cap *= 2;
                args = (ASTNode**)realloc(args, sizeof(ASTNode*) * cap);
            }
            args[argc++] = arg;
        } while (match(tokens, TOKEN_COMMA));
    }
    match(tokens, TOKEN_RPAREN);
    return create_call_node(name, args, argc);
}

static ASTNode* primary(TokenList* tokens) {
    Token t = advance(tokens);
    if (t.type == TOKEN_NUMBER) {
        return create_number_node(t.value);
    }
    if (t.type == TOKEN_STRING) {
        return create_string_node(t.lexeme);
    }
    if (t.type == TOKEN_IDENTIFIER) {
        if (match(tokens, TOKEN_LPAREN)) {
            return parse_call(tokens, t.lexeme);
        }
        return create_variable_node(t.lexeme);
    }
    if (t.type == TOKEN_LPAREN) {
        ASTNode* expr = expression(tokens);
        match(tokens, TOKEN_RPAREN);
        return expr;
    }
    return NULL;
}

// Meno unario: -x diventa 0 - x
static ASTNode* unary(TokenList* tokens) {
    if (match(tokens, TOKEN_MINUS)) {
        ASTNode* operand = unary(tokens);
        return create_binary_node(TOKEN_MINUS, create_number_node(0.0), operand);
    }
    return primary(tokens);
}

static ASTNode* factor(TokenList* tokens) {
    ASTNode* left = unary(tokens);
    while (peek(tokens).type == TOKEN_STAR || peek(tokens).type == TOKEN_SLASH) {
        TokenType op = advance(tokens).type;
        ASTNode* right = unary(tokens);
        left = create_binary_node(op, left, right);
    }
    return left;
}

static ASTNode* term(TokenList* tokens) {
    ASTNode* left = factor(tokens);
    while (peek(tokens).type == TOKEN_PLUS || peek(tokens).type == TOKEN_MINUS) {
        TokenType op = advance(tokens).type;
        ASTNode* right = factor(tokens);
        left = create_binary_node(op, left, right);
    }
    return left;
}

static ASTNode* comparison(TokenList* tokens) {
    ASTNode* left = term(tokens);
    while (peek(tokens).type == TOKEN_GREATER || peek(tokens).type == TOKEN_LESS ||
           peek(tokens).type == TOKEN_GREATER_EQUAL || peek(tokens).type == TOKEN_LESS_EQUAL) {
        TokenType op = advance(tokens).type;
        ASTNode* right = term(tokens);
        left = create_binary_node(op, left, right);
    }
    return left;
}

static ASTNode* equality(TokenList* tokens) {
    ASTNode* left = comparison(tokens);
    while (peek(tokens).type == TOKEN_EQUAL || peek(tokens).type == TOKEN_NOT_EQUAL) {
        TokenType op = advance(tokens).type;
        ASTNode* right = comparison(tokens);
        left = create_binary_node(op, left, right);
    }
    return left;
}

static ASTNode* expression(TokenList* tokens) {
    return equality(tokens);
}

static ASTNode* function_declaration(TokenList* tokens) {
    int nested = function_depth > 0;
    int line = peek(tokens).line;

    Token name = advance(tokens);   // Identificatore
    match(tokens, TOKEN_LPAREN);

    size_t pcap = 4, pcount = 0;
    char** params = (char**)malloc(sizeof(char*) * pcap);
    if (peek(tokens).type != TOKEN_RPAREN) {
        do {
            Token p = advance(tokens);
            if (p.type != TOKEN_IDENTIFIER) break;
            if (pcount >= pcap) {
                pcap *= 2;
                params = (char**)realloc(params, sizeof(char*) * pcap);
            }
            params[pcount++] = strdup(p.lexeme);
        } while (match(tokens, TOKEN_COMMA));
    }
    match(tokens, TOKEN_RPAREN);

    function_depth++;
    ASTNode* body = statement(tokens);
    function_depth--;

    ASTNode* fn = create_func_decl_node(name.lexeme, params, pcount, body);
    fn->line = line;

    if (nested) {
        fprintf(stderr, "Errore di sintassi (riga %d): le funzioni vanno definite al livello superiore, "
                        "non dentro altre funzioni\n", line);
        free_ast(fn);
        return create_number_node(0.0);   // nodo neutro: NULL farebbe saltare il token successivo
    }
    return fn;
}

static ASTNode* statement(TokenList* tokens) {
    if (match(tokens, TOKEN_LET)) {
        Token name = advance(tokens); // Identificatore
        match(tokens, TOKEN_ASSIGN);
        ASTNode* init = expression(tokens);
        match(tokens, TOKEN_SEMICOLON);
        return create_var_decl_node(name.lexeme, init);
    }

    if (match(tokens, TOKEN_PRINT)) {
        ASTNode* expr = expression(tokens);
        match(tokens, TOKEN_SEMICOLON);
        return create_print_node(expr);
    }

    if (match(tokens, TOKEN_LBRACE)) {
        return parse_statements(tokens, 1);   // legge fino alla '}'
    }

    if (match(tokens, TOKEN_IF)) {
        match(tokens, TOKEN_LPAREN);
        ASTNode* cond = expression(tokens);
        match(tokens, TOKEN_RPAREN);
        ASTNode* then_branch = statement(tokens);
        ASTNode* else_branch = NULL;
        if (match(tokens, TOKEN_ELSE)) {
            else_branch = statement(tokens);
        }
        return create_if_node(cond, then_branch, else_branch);
    }

    if (match(tokens, TOKEN_WHILE)) {
        match(tokens, TOKEN_LPAREN);
        ASTNode* cond = expression(tokens);
        match(tokens, TOKEN_RPAREN);
        ASTNode* body = statement(tokens);
        return create_while_node(cond, body);
    }

    if (match(tokens, TOKEN_FN)) {
        return function_declaration(tokens);
    }

    if (peek(tokens).type == TOKEN_RETURN) {
        int line = advance(tokens).line;
        ASTNode* value = NULL;
        TokenType next = peek(tokens).type;
        if (next != TOKEN_SEMICOLON && next != TOKEN_RBRACE && next != TOKEN_EOF) {
            value = expression(tokens);
        }
        match(tokens, TOKEN_SEMICOLON);
        ASTNode* ret = create_return_node(value);
        ret->line = line;
        return ret;
    }

    // Assegnamento: identificatore '=' espressione
    if (peek(tokens).type == TOKEN_IDENTIFIER && peek_at(tokens, 1).type == TOKEN_ASSIGN) {
        Token name = advance(tokens);
        advance(tokens);                       // consuma '='
        ASTNode* value = expression(tokens);
        match(tokens, TOKEN_SEMICOLON);
        return create_assign_node(name.lexeme, value);
    }

    // Espressione usata come istruzione (es. una chiamata: saluta("Mario");)
    TokenType k = peek(tokens).type;
    if (k == TOKEN_IDENTIFIER || k == TOKEN_NUMBER || k == TOKEN_STRING ||
        k == TOKEN_LPAREN || k == TOKEN_MINUS) {
        ASTNode* expr = expression(tokens);
        match(tokens, TOKEN_SEMICOLON);
        return expr;
    }

    return NULL;
}

static ASTNode* parse_statements(TokenList* tokens, int in_braces) {
    ASTNode* block = new_node(NODE_BLOCK);
    block->block.count = 0;

    size_t cap = 8;
    block->block.statements = (ASTNode**)malloc(sizeof(ASTNode*) * cap);

    while (peek(tokens).type != TOKEN_EOF) {
        if (in_braces && peek(tokens).type == TOKEN_RBRACE) {
            advance(tokens);                  // consuma la '}' e chiude il blocco
            break;
        }

        ASTNode* stmt = statement(tokens);
        if (stmt) {
            if (block->block.count >= cap) {
                cap *= 2;
                block->block.statements = (ASTNode**)realloc(
                    block->block.statements, sizeof(ASTNode*) * cap);
            }
            block->block.statements[block->block.count++] = stmt;
        } else {
            advance(tokens);                  // token non riconosciuto (es. ';' isolato)
        }
    }

    return block;
}

ASTNode* parse(TokenList* tokens) {
    current_token = 0;
    function_depth = 0;
    return parse_statements(tokens, 0);
}

void free_ast(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_STRING:
            free(node->str_value);
            break;
        case NODE_VARIABLE:
            free(node->var_name);
            break;
        case NODE_BINARY_OP:
            free_ast(node->binary.left);
            free_ast(node->binary.right);
            break;
        case NODE_VAR_DECL:
            free(node->var_decl.var_name);
            free_ast(node->var_decl.initializer);
            break;
        case NODE_ASSIGN:
            free(node->assign.var_name);
            free_ast(node->assign.value);
            break;
        case NODE_PRINT:
        case NODE_RETURN:
            free_ast(node->expr);                  // free_ast(NULL) è già gestito
            break;
        case NODE_IF:
            free_ast(node->if_stmt.condition);
            free_ast(node->if_stmt.then_branch);
            free_ast(node->if_stmt.else_branch);
            break;
        case NODE_WHILE:
            free_ast(node->while_stmt.condition);
            free_ast(node->while_stmt.body);
            break;
        case NODE_BLOCK:
            for (size_t i = 0; i < node->block.count; i++) {
                free_ast(node->block.statements[i]);
            }
            free(node->block.statements);
            break;
        case NODE_CALL:
            free(node->call.name);
            for (size_t i = 0; i < node->call.argc; i++) {
                free_ast(node->call.args[i]);
            }
            free(node->call.args);
            break;
        case NODE_FUNC_DECL:
            // Una funzione registrata appartiene alla tabella dell'interprete:
            // deve sopravvivere al programma che l'ha definita (utile nella shell ViKi).
            if (node->func.registered) return;
            free(node->func.name);
            for (size_t i = 0; i < node->func.param_count; i++) {
                free(node->func.params[i]);
            }
            free(node->func.params);
            free_ast(node->func.body);
            break;
        default: break;
    }
    free(node);
}

void free_ast_owned(ASTNode* node) {
    if (node && node->type == NODE_FUNC_DECL) node->func.registered = 0;
    free_ast(node);
}
