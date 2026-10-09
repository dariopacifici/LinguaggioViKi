#define _POSIX_C_SOURCE 200809L
#include "interpreter.h"

#define MAX_CALL_DEPTH 500

// Uno scope è una lista di variabili. Esiste lo scope globale e, durante
// una chiamata di funzione, uno scope locale che nasce e muore con la chiamata.
typedef struct {
    Symbol* vars;
} Scope;

typedef struct Func {
    char* name;
    ASTNode* node;          // NODE_FUNC_DECL, posseduto dalla tabella
    struct Func* next;
} Func;

static Scope  global_scope   = { NULL };
static Scope* current_scope  = &global_scope;
static Func*  functions      = NULL;

// Vecchie versioni di funzioni ridefinite: possono essere ancora referenziate
// dall'AST del programma in esecuzione, quindi si liberano solo alla chiusura.
typedef struct Retired {
    ASTNode* node;
    struct Retired* next;
} Retired;
static Retired* retired_functions = NULL;
static int    call_depth     = 0;

// Gestione di 'return': il flag interrompe blocchi e cicli fino alla chiamata
static int   returning       = 0;
static Value return_value;

static Value make_number(double n) {
    Value v; v.type = VAL_NUMBER; v.as.number = n; return v;
}

static Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b ? 1 : 0;
    return v;
}

static Value make_string(char* s) {          // prende possesso di s
    Value v; v.type = VAL_STRING; v.as.string = s; return v;
}

static Value copy_value(Value v) {
    if (v.type == VAL_STRING) return make_string(strdup(v.as.string));
    return v;
}

void free_value(Value v) {
    if (v.type == VAL_STRING) free(v.as.string);
}

static char* value_to_cstr(Value v) {
    if (v.type == VAL_STRING) return strdup(v.as.string);
    char buf[64];
    snprintf(buf, sizeof buf, "%g", v.as.number);
    return strdup(buf);
}

static int is_truthy(Value v) {
    if (v.type == VAL_BOOL)   return v.as.boolean;
    if (v.type == VAL_NUMBER) return v.as.number != 0.0;
    if (v.type == VAL_STRING) return v.as.string && v.as.string[0] != '\0';
    return 0;
}

// ---------- Variabili e scope ----------

static Symbol* find_in(Symbol* list, const char* name) {
    for (Symbol* s = list; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

// Prima lo scope corrente, poi quello globale
static Symbol* lookup(const char* name) {
    Symbol* s = find_in(current_scope->vars, name);
    if (!s && current_scope != &global_scope) s = find_in(global_scope.vars, name);
    return s;
}

// Dichiara (o ridefinisce) una variabile nello scope corrente
void set_variable(const char* name, Value value) {
    Symbol* s = find_in(current_scope->vars, name);
    if (s) {
        free_value(s->value);
        s->value = value;
        return;
    }
    Symbol* new_sym = (Symbol*)malloc(sizeof(Symbol));
    new_sym->name = strdup(name);
    new_sym->value = value;
    new_sym->next = current_scope->vars;
    current_scope->vars = new_sym;
}

// Assegna a una variabile già esistente (locale, altrimenti globale)
static int assign_variable(const char* name, Value value) {
    Symbol* s = lookup(name);
    if (!s) {
        free_value(value);
        return 0;
    }
    free_value(s->value);
    s->value = value;
    return 1;
}

Value get_variable(const char* name) {
    Symbol* s = lookup(name);
    if (s) return copy_value(s->value);
    fprintf(stderr, "Errore Runtime: variabile '%s' non trovata!\n", name);
    return make_number(0.0);
}

static void free_symbols(Symbol* list) {
    while (list) {
        Symbol* tmp = list;
        list = list->next;
        free(tmp->name);
        free_value(tmp->value);
        free(tmp);
    }
}

// ---------- Funzioni ----------

static Func* find_function(const char* name) {
    for (Func* f = functions; f; f = f->next) {
        if (strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

static void register_function(ASTNode* node) {
    Func* existing = find_function(node->func.name);
    if (existing) {
        if (existing->node == node) return;         // stessa dichiarazione rieseguita
        Retired* r = (Retired*)malloc(sizeof(Retired));
        r->node = existing->node;                   // resta registrata: la libera free_functions
        r->next = retired_functions;
        retired_functions = r;
        existing->node = node;
    } else {
        Func* f = (Func*)malloc(sizeof(Func));
        f->name = strdup(node->func.name);
        f->node = node;
        f->next = functions;
        functions = f;
    }
    node->func.registered = 1;
}

static void free_functions(void) {
    Func* curr = functions;
    while (curr) {
        Func* tmp = curr;
        curr = curr->next;
        free_ast_owned(tmp->node);
        free(tmp->name);
        free(tmp);
    }
    functions = NULL;

    while (retired_functions) {
        Retired* r = retired_functions;
        retired_functions = r->next;
        free_ast_owned(r->node);
        free(r);
    }
}

void free_symbol_table(void) {
    free_symbols(global_scope.vars);
    global_scope.vars = NULL;
    current_scope = &global_scope;
    free_functions();
    call_depth = 0;
    returning = 0;
}

static Value call_function(ASTNode* node) {
    Func* f = find_function(node->call.name);
    if (!f) {
        fprintf(stderr, "Errore Runtime: funzione '%s' non definita\n", node->call.name);
        return make_number(0.0);
    }

    FuncDeclNode* fn = &f->node->func;
    if (node->call.argc != fn->param_count) {
        fprintf(stderr, "Errore Runtime: '%s' richiede %zu argomenti, ne sono stati passati %zu\n",
                fn->name, fn->param_count, node->call.argc);
        return make_number(0.0);
    }

    if (call_depth >= MAX_CALL_DEPTH) {
        fprintf(stderr, "Errore Runtime: troppe chiamate annidate in '%s' (limite %d)\n",
                fn->name, MAX_CALL_DEPTH);
        return make_number(0.0);
    }

    // Gli argomenti si valutano nello scope del chiamante
    Value* args = node->call.argc ? (Value*)malloc(sizeof(Value) * node->call.argc) : NULL;
    for (size_t i = 0; i < node->call.argc; i++) {
        args[i] = evaluate(node->call.args[i]);
    }

    Scope local = { NULL };
    Scope* saved = current_scope;
    current_scope = &local;
    for (size_t i = 0; i < fn->param_count; i++) {
        set_variable(fn->params[i], args[i]);       // il parametro prende possesso del valore
    }
    free(args);

    call_depth++;
    free_value(evaluate(fn->body));
    call_depth--;

    Value result = make_number(0.0);
    if (returning) {
        result = return_value;
        returning = 0;
    }

    free_symbols(local.vars);
    current_scope = saved;
    return result;
}

// ---------- Valutazione ----------

Value evaluate(ASTNode* node) {
    if (!node) return make_number(0.0);

    switch (node->type) {
        case NODE_NUMBER:
            return make_number(node->num_value);

        case NODE_STRING:
            return make_string(strdup(node->str_value));

        case NODE_VARIABLE:
            return get_variable(node->var_name);

        case NODE_BINARY_OP: {
            Value left  = evaluate(node->binary.left);
            Value right = evaluate(node->binary.right);
            Value result = make_number(0.0);
            TokenType op = node->binary.op;

            if (op == TOKEN_PLUS &&
                (left.type == VAL_STRING || right.type == VAL_STRING)) {
                char* a = value_to_cstr(left);
                char* b = value_to_cstr(right);
                char* joined = (char*)malloc(strlen(a) + strlen(b) + 1);
                strcpy(joined, a);
                strcat(joined, b);
                free(a);
                free(b);
                result = make_string(joined);
            } else if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL) {
                int eq;
                if (left.type != right.type)         eq = 0;
                else if (left.type == VAL_STRING)    eq = (strcmp(left.as.string, right.as.string) == 0);
                else                                 eq = (left.as.number == right.as.number);
               /* result = make_number((op == TOKEN_EQUAL) ? eq : !eq); */
               result = make_bool((op == TOKEN_EQUAL) ? eq : !eq);
            } else if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                double l = left.as.number;
                double r = right.as.number;
                switch (op) {
                    case TOKEN_PLUS:          result = make_number(l + r); break;
                    case TOKEN_MINUS:         result = make_number(l - r); break;
                    case TOKEN_STAR:          result = make_number(l * r); break;
                    case TOKEN_SLASH:
                        if (r == 0.0) {
                            fprintf(stderr, "Errore Runtime: divisione per zero\n");
                        } else {
                            result = make_number(l / r);
                        }
                        break;
                    case TOKEN_GREATER:       result = make_number(l > r  ? 1.0 : 0.0); break;
                    case TOKEN_LESS:          result = make_number(l < r  ? 1.0 : 0.0); break;
                    case TOKEN_GREATER_EQUAL: result = make_number(l >= r ? 1.0 : 0.0); break;
                    case TOKEN_LESS_EQUAL:    result = make_number(l <= r ? 1.0 : 0.0); break;
                    default: break;
                }
            } else {
                fprintf(stderr, "Errore Runtime: operazione non valida su stringhe\n");
            }

            free_value(left);
            free_value(right);
            return result;
        }

        case NODE_VAR_DECL: {
            Value val = evaluate(node->var_decl.initializer);
            set_variable(node->var_decl.var_name, copy_value(val));
            return val;
        }

        case NODE_ASSIGN: {
            Value val = evaluate(node->assign.value);
            if (!assign_variable(node->assign.var_name, copy_value(val))) {
                fprintf(stderr, "Errore Runtime: variabile '%s' non dichiarata (usa 'let')\n",
                        node->assign.var_name);
            }
            return val;
        }

        case NODE_PRINT: {
            Value val = evaluate(node->expr);
            if (val.type == VAL_STRING) printf(">> %s\n", val.as.string);
            else                        printf(">> %g\n", val.as.number);
            return val;
        }

        case NODE_IF: {
            Value cond = evaluate(node->if_stmt.condition);
            int truthy = is_truthy(cond);
            free_value(cond);

            if (truthy) {
                free_value(evaluate(node->if_stmt.then_branch));
            } else if (node->if_stmt.else_branch) {
                free_value(evaluate(node->if_stmt.else_branch));
            }
            return make_number(0.0);
        }

        case NODE_WHILE: {
            while (!returning) {
                Value cond = evaluate(node->while_stmt.condition);
                int truthy = is_truthy(cond);
                free_value(cond);
                if (!truthy) break;
                free_value(evaluate(node->while_stmt.body));
            }
            return make_number(0.0);
        }

        case NODE_BLOCK: {
            for (size_t i = 0; i < node->block.count; i++) {
                free_value(evaluate(node->block.statements[i]));
                if (returning) break;
            }
            return make_number(0.0);
        }

        case NODE_FUNC_DECL:
            register_function(node);
            return make_number(0.0);

        case NODE_CALL:
            return call_function(node);

        case NODE_RETURN: {
            Value val = node->expr ? evaluate(node->expr) : make_number(0.0);
            if (call_depth == 0) {
                fprintf(stderr, "Errore Runtime (riga %d): 'return' fuori da una funzione\n", node->line);
                free_value(val);
                return make_number(0.0);
            }
            return_value = val;
            returning = 1;
            return make_number(0.0);
        }

        default: break;
    }
    return make_number(0.0);
}
