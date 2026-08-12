#define MAX_TIMERS 32
typedef struct {
    int  id;
    long deadline_ms;
    long interval_ms;
    char fn_name[64];
    int  active;
} JSTimer;

static JSTimer js_timers[MAX_TIMERS];
static int     js_timer_next_id = 1;

#define MAX_SCRIPT_BLOCKS 16
#define SCRIPT_BLOCK_SIZE  4096
char js_script_blocks[MAX_SCRIPT_BLOCKS][SCRIPT_BLOCK_SIZE];
int js_script_block_count = 0;

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
    TokenType type;
    char text[64];   // pro TOK_IDENT / TOK_STRING (uřezáno na 63 znaků)
    double num;       // pro TOK_NUMBER
} Token;

#define MAX_TOKENS 2048
Token js_tokens[MAX_TOKENS];
int js_token_count = 0;

typedef struct { const char* kw; TokenType type; } KeywordEntry;
static const KeywordEntry js_keywords[] = {
    {"var", TOK_KW_VAR}, {"let", TOK_KW_LET}, {"const", TOK_KW_CONST},
    {"function", TOK_KW_FUNCTION}, {"return", TOK_KW_RETURN},
    {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE},
    {"for", TOK_KW_FOR}, {"while", TOK_KW_WHILE},
    {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
    {"null", TOK_KW_NULL}, {"undefined", TOK_KW_UNDEFINED},
};
#define JS_KEYWORD_COUNT (sizeof(js_keywords)/sizeof(js_keywords[0]))
// ----------------------------------------------------------------------------
// AST
// ----------------------------------------------------------------------------
typedef enum {
    // výrazy
    AST_NUM, AST_STR, AST_BOOL, AST_NULL, AST_UNDEF, AST_IDENT,
    AST_BINOP, AST_UNOP, AST_LOGICAL, AST_ASSIGN, AST_ASSIGN_OP,
    AST_CALL, AST_MEMBER, AST_INDEX, AST_FUNC_EXPR, AST_POSTFIX,
    // příkazy
    AST_VAR_DECL, AST_EXPR_STMT, AST_BLOCK, AST_IF, AST_FOR, AST_WHILE,
    AST_RETURN, AST_FUNC_DECL, AST_PROGRAM
} AstType;

typedef struct AstNode AstNode;

#define MAX_AST_NODES 4096
#define MAX_AST_CHILDREN 32   //  pro výrazy i středně velké bloky/programy (~1MB pro celý pool)

struct AstNode {
    AstType type;
    char str_val[64];     // identifikátor / string literal / operátor jméno / property jméno
    double num_val;
    AstNode* children[MAX_AST_CHILDREN];
    int child_count;
    // jména parametrů (do 8) + tělo je children[0] (AST_BLOCK)
    char params[8][32];
    int param_count;
};

AstNode js_ast_pool[MAX_AST_NODES];
int js_ast_count = 0;


typedef struct {
    char name[32];
    JSValue value;
} JSVar;

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


JSValue js_return_value;
int js_returning = 0;