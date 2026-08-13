#ifndef JS_LEXER_H
#define JS_LEXER_H


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
int js_is_ident_start(char c);
int js_is_ident_char(char c) ;
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



#endif /* JS_LEXER_H */