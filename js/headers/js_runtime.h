    #ifndef JS_LEXER_H
#define JS_LEXER_H
typedef struct {
    JSValueType type;
    double num;
    char* str;           
    DOMNode* dom;         
} JSValue;
int js_script_block_count = 0;
#define MAX_TOKENS 2048

typedef enum {
    TOK_EOF, TOK_NUMBER, TOK_STRING, TOK_IDENT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMI, TOK_COMMA, TOK_DOT,
    TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NEQ, TOK_EQ_STRICT, TOK_NEQ_STRICT,
    TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_INC, TOK_DEC,
    TOK_KW_VAR, TOK_KW_LET, TOK_KW_CONST, TOK_KW_FUNCTION, TOK_KW_RETURN,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_FOR, TOK_KW_WHILE,
    TOK_KW_TRUE, TOK_KW_FALSE, TOK_KW_NULL, TOK_KW_UNDEFINED,
    TOK_UNKNOWN
} TokenType;
typedef struct {
    char name[32];
    JSValue value;
} JSVar;
typedef struct {
    TokenType type;
    char text[64];   
    double num;       
} Token;

typedef struct { 
    const char* kw; 
    TokenType type; 
} KeywordEntry;

extern Token js_tokens[MAX_TOKENS];
extern int js_token_count;

extern const KeywordEntry js_keywords[];
extern const int JS_KEYWORD_COUNT;


#define MAX_TIMERS 32
typedef struct {
    int  id;
    long deadline_ms;
    long interval_ms;
    char fn_name[64];
    int  active;
} JSTimer;

 JSTimer js_timers[MAX_TIMERS];
 int     js_timer_next_id = 1;
 #define MAX_SCRIPT_BLOCKS 16
 #define SCRIPT_BLOCK_SIZE  4096
 char js_script_blocks[MAX_SCRIPT_BLOCKS][SCRIPT_BLOCK_SIZE];
 #define JS_STRING_POOL_SIZE 16384
 char js_string_pool[JS_STRING_POOL_SIZE];
#define MAX_GLOBALS 128
#define MAX_LOCALS  64
JSVar js_globals[MAX_GLOBALS];
int js_global_count = 0;

JSVar js_locals[MAX_LOCALS];
int js_local_count = 0;
int js_in_function = 0; // 0 = jsme na top-levelu, 1 = uvnitř volání funkce (lokální scope aktivní)

#define MAX_JS_FUNCS 32
typedef struct {
    char name[32];
    AstNode* decl; // AST_FUNC_DECL nebo AST_FUNC_EXPR uzel (obsahuje params + tělo)
} JSFunc;
JSFunc js_functions[MAX_JS_FUNCS];
int js_func_count = 0;
int js_timers_tick(void);
int js_fire_click_bubbling(DOMNode* page_root, DOMNode* node);
int dom_collect_ancestor_chain(DOMNode* root, DOMNode* target, DOMNode** out_chain, int max_len);
DOMNode* dom_find_at_point(DOMNode* node, int doc_x, int doc_y);
DOMNode* find_enclosing_form(DOMNode* page_root, DOMNode* target);
void url_encode(const char* src, char* out, int out_size) ;
JSValue js_return_value;
int js_returning = 0;

#endif /* JS_LEXER_H */