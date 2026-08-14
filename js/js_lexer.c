#include "headers/js_lexer.h"
#include "../utils/string/str_utils.h"
#include <stddef.h>

// Externí funkce, které lexer potřebuje (nebo přidej hlavičku, kde jsou definované)

// --- FYZICKÁ ALOKACE GLOBÁLNÍCH PROMĚNNÝCH LEXERU ---
Token js_tokens[MAX_TOKENS];
int js_token_count = 0;

const KeywordEntry js_keywords[] = {
    {"var", TOK_KW_VAR}, {"let", TOK_KW_LET}, {"const", TOK_KW_CONST},
    {"function", TOK_KW_FUNCTION}, {"return", TOK_KW_RETURN},
    {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE},
    {"for", TOK_KW_FOR}, {"while", TOK_KW_WHILE},
    {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
    {"null", TOK_KW_NULL}, {"undefined", TOK_KW_UNDEFINED},
};
const int JS_KEYWORD_COUNT = sizeof(js_keywords)/sizeof(js_keywords[0]);

// --- IMPLEMENTACE FUNKCÍ ---
int js_is_ident_start(char c) { 
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'; 
}
int js_is_ident_char(char c)  { 
    return js_is_ident_start(c) || is_digit_c(c); 
}

int js_tokenize(const char* src) {
    js_token_count = 0;
    const char* p = src;

    while (*p && js_token_count < MAX_TOKENS - 1) {
        // mezery
        if (is_space_c(*p)) { p++; continue; }

        // komentáře // a /* */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        Token tok;
        tok.text[0] = '\0';
        tok.num = 0;

        // čísla
        if (is_digit_c(*p)) {
            double val = 0;
            while (is_digit_c(*p)) { val = val * 10 + (*p - '0'); p++; }
            if (*p == '.') {
                p++;
                double frac = 0.1;
                while (is_digit_c(*p)) { val += (*p - '0') * frac; frac *= 0.1; p++; }
            }
            tok.type = TOK_NUMBER;
            tok.num = val;
            js_tokens[js_token_count++] = tok;
            continue;
        }

        // stringy "..." nebo chary '...'
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            int n = 0;
            while (*p && *p != quote && n < 63) {
                if (*p == '\\' && p[1]) { //  \" \' \\ \n
                    p++;
                    char c = *p;
                    if (c == 'n') tok.text[n++] = '\n';
                    else if (c == 't') tok.text[n++] = '\t';
                    else tok.text[n++] = c;
                    p++;
                } else {
                    tok.text[n++] = *p;
                    p++;
                }
            }
            tok.text[n] = '\0';
            if (*p == quote) p++;
            tok.type = TOK_STRING;
            js_tokens[js_token_count++] = tok;
            continue;
        }

        // identifikátory 
        if (js_is_ident_start(*p)) {
            int n = 0;
            while (js_is_ident_char(*p) && n < 63) { tok.text[n++] = *p; p++; }
            tok.text[n] = '\0';

            TokenType kw_type = TOK_IDENT;
            for (unsigned i = 0; i < JS_KEYWORD_COUNT; i++) {
                if (str_eq(tok.text, js_keywords[i].kw)) { kw_type = js_keywords[i].type; break; }
            }
            tok.type = kw_type;
            js_tokens[js_token_count++] = tok;
            continue;
        }

        // operátory a interpunkce 
        if (p[0] == '=' && p[1] == '=' && p[2] == '=') { tok.type = TOK_EQ_STRICT; p += 3; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '!' && p[1] == '=' && p[2] == '=') { tok.type = TOK_NEQ_STRICT; p += 3; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '=' && p[1] == '=') { tok.type = TOK_EQ; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '!' && p[1] == '=') { tok.type = TOK_NEQ; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '<' && p[1] == '=') { tok.type = TOK_LE; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '>' && p[1] == '=') { tok.type = TOK_GE; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '&' && p[1] == '&') { tok.type = TOK_AND; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '|' && p[1] == '|') { tok.type = TOK_OR;  p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '+' && p[1] == '+') { tok.type = TOK_INC; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '-' && p[1] == '-') { tok.type = TOK_DEC; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '+' && p[1] == '=') { tok.type = TOK_PLUS_ASSIGN; p += 2; js_tokens[js_token_count++] = tok; continue; }
        if (p[0] == '-' && p[1] == '=') { tok.type = TOK_MINUS_ASSIGN; p += 2; js_tokens[js_token_count++] = tok; continue; }

        char c = *p;
        TokenType single = TOK_UNKNOWN;
        switch (c) {
            case '(': single = TOK_LPAREN; break;
            case ')': single = TOK_RPAREN; break;
            case '{': single = TOK_LBRACE; break;
            case '}': single = TOK_RBRACE; break;
            case '[': single = TOK_LBRACKET; break;
            case ']': single = TOK_RBRACKET; break;
            case ';': single = TOK_SEMI; break;
            case ',': single = TOK_COMMA; break;
            case '.': single = TOK_DOT; break;
            case '=': single = TOK_ASSIGN; break;
            case '+': single = TOK_PLUS; break;
            case '-': single = TOK_MINUS; break;
            case '*': single = TOK_STAR; break;
            case '/': single = TOK_SLASH; break;
            case '%': single = TOK_PERCENT; break;
            case '<': single = TOK_LT; break;
            case '>': single = TOK_GT; break;
            case '!': single = TOK_NOT; break;
            default: single = TOK_UNKNOWN; break;
        }
        tok.type = single;
        p++;
        js_tokens[js_token_count++] = tok;
    }

    Token eof_tok; eof_tok.type = TOK_EOF; eof_tok.text[0] = '\0'; eof_tok.num = 0;
    js_tokens[js_token_count++] = eof_tok;
    return js_token_count;
}