#include <stdint.h>

#include "js.h"
#include "DOM.h"
#include "css.h"
#include <stddef.h>
extern void compute_style(DOMNode* n, ComputedStyle* parent_style);
extern long sys_get_time_ms(void);
extern void str_copy(char* dest, const char* src, size_t n);
extern int str_eq(const char* a, const char* b);
extern int str_len(const char* s);
extern void sys_cookie_set(const char *domain, const char *cookie_str) ;
extern int is_digit_c(char c), current_is_https;
extern int is_space_c(char c);
extern int skip_spaces(const char** p);
extern int parse_int(const char** p);
extern int parse_ipv4(const char* s), sys_dns_resolve(const char *domain);
extern int has_class(const char* class_attr, const char* target);
extern DOMNode* alloc_node();
extern int layout_dirty;
extern void resolve_relative_url(const char* href, char* out, int out_size);
extern int parse_url(const char* url, char* out_domain, char* out_path, int* out_is_https, int* out_port, int* out_is_ip);
extern int sys_https_get(uint32_t ip, const char *domain, uint16_t port, const char *path, char *buf, int max_len);
extern int sys_http_get(uint32_t ip, uint16_t port, const char *path, char *buf, int max_len) ;
extern void sys_cookie_get(const char *domain, int is_https, char *out, int out_size);
extern uint32_t target_ip, current_server_ip, parse_css_color(const char** p);
extern int parse_css_length(const char** p);
extern char* current_domain;
static int js_timer_register(const char* fn_name, long delay_ms, long interval_ms) {
    long now = sys_get_time_ms();
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (js_timers[i].active) continue;
        js_timers[i].id          = js_timer_next_id++;
        js_timers[i].deadline_ms = now + delay_ms;
        js_timers[i].interval_ms = interval_ms;
        js_timers[i].active      = 1;
        str_copy(js_timers[i].fn_name, fn_name, sizeof(js_timers[i].fn_name));
        return js_timers[i].id;
    }
    return -1;
}

static void js_timer_clear(int id) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (js_timers[i].active && js_timers[i].id == id) {
            js_timers[i].active = 0;
            return;
        }
    }
}


int js_timers_tick(void);

void js_timers_reset(void) {
    for (int i = 0; i < MAX_TIMERS; i++) js_timers[i].active = 0;
}

void js_collect_script(const char* src, int len) {
    if (js_script_block_count >= MAX_SCRIPT_BLOCKS) return;
    if (len > SCRIPT_BLOCK_SIZE - 1) len = SCRIPT_BLOCK_SIZE - 1;
    char* dst = js_script_blocks[js_script_block_count];
    for (int i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = '\0';
    js_script_block_count++;
}

int js_is_ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'; }
int js_is_ident_char(char c)  { return js_is_ident_start(c) || is_digit_c(c); }

// Naparsuje celý JS zdrojový text na tokeny do js_tokens[]. Vrací počet tokenů.
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
// ============================================================================
// 10. JAVASCRIPT ENGINE — HODNOTY (JSValue) A AST
// ============================================================================
typedef enum {
    JSV_UNDEFINED, JSV_NULL, JSV_BOOL, JSV_NUMBER, JSV_STRING, JSV_DOMNODE
} JSValueType;

#define JS_STRING_POOL_SIZE 16384
char js_string_pool[JS_STRING_POOL_SIZE];
int js_string_pool_used = 0;


char* js_alloc_string(const char* src, int len) {
    if (js_string_pool_used + len + 1 > JS_STRING_POOL_SIZE) {
        // pool je plný - vrátíme prázdný string
        return &js_string_pool[JS_STRING_POOL_SIZE - 1];
    }
    char* dst = &js_string_pool[js_string_pool_used];
    for (int i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = '\0';
    js_string_pool_used += len + 1;
    return dst;
}
char* js_alloc_string_z(const char* src) { return js_alloc_string(src, str_len(src)); }

typedef struct {
    JSValueType type;
    double num;
    char* str;           
    DOMNode* dom;         
} JSValue;

JSValue js_undefined(void) { JSValue v; v.type = JSV_UNDEFINED; v.num = 0; v.str = 0; v.dom = 0; return v; }
JSValue js_null_val(void)  { JSValue v; v.type = JSV_NULL; v.num = 0; v.str = 0; v.dom = 0; return v; }
JSValue js_bool_val(int b)  { JSValue v; v.type = JSV_BOOL; v.num = b ? 1 : 0; v.str = 0; v.dom = 0; return v; }
JSValue js_num_val(double n) { JSValue v; v.type = JSV_NUMBER; v.num = n; v.str = 0; v.dom = 0; return v; }
JSValue js_str_val(char* s) { JSValue v; v.type = JSV_STRING; v.num = 0; v.str = s; v.dom = 0; return v; }
JSValue js_dom_val(DOMNode* n) { JSValue v; v.type = JSV_DOMNODE; v.num = 0; v.str = 0; v.dom = n; return v; }

int js_truthy(JSValue v) {
    switch (v.type) {
        case JSV_UNDEFINED: case JSV_NULL: return 0;
        case JSV_BOOL: return v.num != 0;
        case JSV_NUMBER: return v.num != 0;
        case JSV_STRING: return v.str && v.str[0] != '\0';
        case JSV_DOMNODE: return v.dom != 0;
    }
    return 0;
}

double js_to_number(JSValue v) {
    switch (v.type) {
        case JSV_NUMBER: return v.num;
        case JSV_BOOL: return v.num;
        case JSV_STRING: { const char* p = v.str; skip_spaces(&p); return (double)parse_int(&p); }
        default: return 0;
    }
}

// Zapíše textovou reprezentaci hodnoty do out (pro console.log, zřetězení '+', innerHTML=...).
void js_to_string(JSValue v, char* out, int out_size) {
    switch (v.type) {
        case JSV_UNDEFINED: str_copy(out, "undefined", out_size); break;
        case JSV_NULL: str_copy(out, "null", out_size); break;
        case JSV_BOOL: str_copy(out, v.num != 0 ? "true" : "false", out_size); break;
        case JSV_STRING: str_copy(out, v.str ? v.str : "", out_size); break;
        case JSV_DOMNODE: str_copy(out, "[object HTMLElement]", out_size); break;
        case JSV_NUMBER: {
            double n = v.num;
            int neg = n < 0; if (neg) n = -n;
            long ip = (long)n;
            double frac = n - (double)ip;
            char buf[32]; int bi = 0;
            if (neg) buf[bi++] = '-';
            char tmp[24]; int ti = 0;
            long t = ip;
            if (t == 0) tmp[ti++] = '0';
            while (t > 0) { tmp[ti++] = '0' + (t % 10); t /= 10; }
            while (ti > 0) buf[bi++] = tmp[--ti];
            if (frac > 0.00001) {
                buf[bi++] = '.';
                for (int k = 0; k < 4; k++) {
                    frac *= 10;
                    int d = (int)frac;
                    buf[bi++] = '0' + d;
                    frac -= d;
                    if (frac < 0.00001) break;
                }
            }
            buf[bi] = '\0';
            str_copy(out, buf, out_size);
            break;
        }
    }
}


AstNode* js_alloc_ast(AstType type) {
    if (js_ast_count >= MAX_AST_NODES) return 0;
    AstNode* n = &js_ast_pool[js_ast_count++];
    n->type = type;
    n->str_val[0] = '\0';
    n->num_val = 0;
    n->child_count = 0;
    n->param_count = 0;
    return n;
}
// ============================================================================
// 11. JAVASCRIPT ENGINE — PARSER (recursive-descent)
// ============================================================================
int js_parse_pos = 0;

Token* js_peek(void) { return &js_tokens[js_parse_pos]; }
Token* js_peek_at(int offset) {
    int idx = js_parse_pos + offset;
    if (idx >= js_token_count) idx = js_token_count - 1;
    return &js_tokens[idx];
}
Token* js_advance(void) {
    Token* t = &js_tokens[js_parse_pos];
    if (js_parse_pos < js_token_count - 1) js_parse_pos++;
    return t;
}
int js_check(TokenType t) { return js_peek()->type == t; }
int js_match(TokenType t) { if (js_check(t)) { js_advance(); return 1; } return 0; }
void js_expect(TokenType t) { if (js_check(t)) js_advance(); /* chybu jen tiše ignorujeme */ }

AstNode* js_parse_expression(void);
AstNode* js_parse_assignment(void);
AstNode* js_parse_statement(void);
AstNode* js_parse_block(void);

// --- primary: čísla, stringy, identifikátory, závorky, function expr ---
AstNode* js_parse_primary(void) {
    Token* t = js_peek();

    if (t->type == TOK_NUMBER) {
        js_advance();
        AstNode* n = js_alloc_ast(AST_NUM);
        n->num_val = t->num;
        return n;
    }
    if (t->type == TOK_STRING) {
        js_advance();
        AstNode* n = js_alloc_ast(AST_STR);
        str_copy(n->str_val, t->text, sizeof(n->str_val));
        return n;
    }
    if (t->type == TOK_KW_TRUE)  { js_advance(); AstNode* n = js_alloc_ast(AST_BOOL); n->num_val = 1; return n; }
    if (t->type == TOK_KW_FALSE) { js_advance(); AstNode* n = js_alloc_ast(AST_BOOL); n->num_val = 0; return n; }
    if (t->type == TOK_KW_NULL)  { js_advance(); return js_alloc_ast(AST_NULL); }
    if (t->type == TOK_KW_UNDEFINED) { js_advance(); return js_alloc_ast(AST_UNDEF); }

    if (t->type == TOK_KW_FUNCTION) {
        js_advance(); // 'function'
        AstNode* fn = js_alloc_ast(AST_FUNC_EXPR);
        if (js_check(TOK_IDENT)) { str_copy(fn->str_val, js_peek()->text, sizeof(fn->str_val)); js_advance(); }
        js_expect(TOK_LPAREN);
        while (!js_check(TOK_RPAREN) && !js_check(TOK_EOF)) {
            if (js_check(TOK_IDENT) && fn->param_count < 8) {
                str_copy(fn->params[fn->param_count++], js_peek()->text, 32);
            }
            js_advance();
            js_match(TOK_COMMA);
        }
        js_expect(TOK_RPAREN);
        fn->children[fn->child_count++] = js_parse_block();
        return fn;
    }

    if (t->type == TOK_IDENT) {
        js_advance();
        AstNode* n = js_alloc_ast(AST_IDENT);
        str_copy(n->str_val, t->text, sizeof(n->str_val));
        return n;
    }

    if (t->type == TOK_LPAREN) {
        js_advance();
        AstNode* inner = js_parse_expression();
        js_expect(TOK_RPAREN);
        return inner;
    }

    // neznámý/neočekávaný token - vrať undefined a posuň se, aby parser nezacyklil
    js_advance();
    return js_alloc_ast(AST_UNDEF);
}

// --- postfix: volání f(...), .property, [index], ++  -- ---
AstNode* js_parse_call_member(void) {
    AstNode* expr = js_parse_primary();

    for (;;) {
        if (js_check(TOK_LPAREN)) {
            js_advance();
            AstNode* call = js_alloc_ast(AST_CALL);
            call->children[call->child_count++] = expr; // callee
            while (!js_check(TOK_RPAREN) && !js_check(TOK_EOF)) {
                if (call->child_count < MAX_AST_CHILDREN) {
                    call->children[call->child_count++] = js_parse_assignment();
                } else {
                    js_parse_assignment(); // přebytečné argumenty zahodíme
                }
                if (!js_match(TOK_COMMA)) break;
            }
            js_expect(TOK_RPAREN);
            expr = call;
            continue;
        }
        if (js_check(TOK_DOT)) {
            js_advance();
            AstNode* member = js_alloc_ast(AST_MEMBER);
            member->children[member->child_count++] = expr;
            if (js_check(TOK_IDENT)) {
                str_copy(member->str_val, js_peek()->text, sizeof(member->str_val));
                js_advance();
            }
            expr = member;
            continue;
        }
        if (js_check(TOK_LBRACKET)) {
            js_advance();
            AstNode* idx = js_alloc_ast(AST_INDEX);
            idx->children[idx->child_count++] = expr;
            idx->children[idx->child_count++] = js_parse_expression();
            js_expect(TOK_RBRACKET);
            expr = idx;
            continue;
        }
        if (js_check(TOK_INC) || js_check(TOK_DEC)) {
            AstNode* post = js_alloc_ast(AST_POSTFIX);
            str_copy(post->str_val, js_check(TOK_INC) ? "++" : "--", sizeof(post->str_val));
            post->children[post->child_count++] = expr;
            js_advance();
            expr = post;
            continue;
        }
        break;
    }
    return expr;
}

// --- unary: !x, -x, ++x, --x ---
AstNode* js_parse_unary(void) {
    if (js_check(TOK_NOT) || js_check(TOK_MINUS) || js_check(TOK_PLUS)) {
        Token* op = js_advance();
        AstNode* n = js_alloc_ast(AST_UNOP);
        n->str_val[0] = (op->type == TOK_NOT) ? '!' : (op->type == TOK_MINUS) ? '-' : '+';
        n->str_val[1] = '\0';
        n->children[n->child_count++] = js_parse_unary();
        return n;
    }
    if (js_check(TOK_INC) || js_check(TOK_DEC)) {
        int is_inc = js_check(TOK_INC);
        js_advance();
        AstNode* n = js_alloc_ast(AST_UNOP);
        str_copy(n->str_val, is_inc ? "++pre" : "--pre", sizeof(n->str_val));
        n->children[n->child_count++] = js_parse_unary();
        return n;
    }
    return js_parse_call_member();
}

AstNode* js_make_binop(const char* op, AstNode* l, AstNode* r) {
    AstNode* n = js_alloc_ast(AST_BINOP);
    str_copy(n->str_val, op, sizeof(n->str_val));
    n->children[n->child_count++] = l;
    n->children[n->child_count++] = r;
    return n;
}
AstNode* js_make_logical(const char* op, AstNode* l, AstNode* r) {
    AstNode* n = js_alloc_ast(AST_LOGICAL);
    str_copy(n->str_val, op, sizeof(n->str_val));
    n->children[n->child_count++] = l;
    n->children[n->child_count++] = r;
    return n;
}

AstNode* js_parse_multiplicative(void) {
    AstNode* left = js_parse_unary();
    for (;;) {
        if (js_check(TOK_STAR)) { js_advance(); left = js_make_binop("*", left, js_parse_unary()); }
        else if (js_check(TOK_SLASH)) { js_advance(); left = js_make_binop("/", left, js_parse_unary()); }
        else if (js_check(TOK_PERCENT)) { js_advance(); left = js_make_binop("%", left, js_parse_unary()); }
        else break;
    }
    return left;
}
AstNode* js_parse_additive(void) {
    AstNode* left = js_parse_multiplicative();
    for (;;) {
        if (js_check(TOK_PLUS)) { js_advance(); left = js_make_binop("+", left, js_parse_multiplicative()); }
        else if (js_check(TOK_MINUS)) { js_advance(); left = js_make_binop("-", left, js_parse_multiplicative()); }
        else break;
    }
    return left;
}
AstNode* js_parse_relational(void) {
    AstNode* left = js_parse_additive();
    for (;;) {
        if (js_check(TOK_LT)) { js_advance(); left = js_make_binop("<", left, js_parse_additive()); }
        else if (js_check(TOK_GT)) { js_advance(); left = js_make_binop(">", left, js_parse_additive()); }
        else if (js_check(TOK_LE)) { js_advance(); left = js_make_binop("<=", left, js_parse_additive()); }
        else if (js_check(TOK_GE)) { js_advance(); left = js_make_binop(">=", left, js_parse_additive()); }
        else break;
    }
    return left;
}
AstNode* js_parse_equality(void) {
    AstNode* left = js_parse_relational();
    for (;;) {
        if (js_check(TOK_EQ) || js_check(TOK_EQ_STRICT)) { js_advance(); left = js_make_binop("==", left, js_parse_relational()); }
        else if (js_check(TOK_NEQ) || js_check(TOK_NEQ_STRICT)) { js_advance(); left = js_make_binop("!=", left, js_parse_relational()); }
        else break;
    }
    return left;
}
AstNode* js_parse_logical_and(void) {
    AstNode* left = js_parse_equality();
    while (js_check(TOK_AND)) { js_advance(); left = js_make_logical("&&", left, js_parse_equality()); }
    return left;
}
AstNode* js_parse_logical_or(void) {
    AstNode* left = js_parse_logical_and();
    while (js_check(TOK_OR)) { js_advance(); left = js_make_logical("||", left, js_parse_logical_and()); }
    return left;
}

AstNode* js_parse_assignment(void) {
    AstNode* left = js_parse_logical_or();
    if (js_check(TOK_ASSIGN)) {
        js_advance();
        AstNode* n = js_alloc_ast(AST_ASSIGN);
        n->children[n->child_count++] = left;
        n->children[n->child_count++] = js_parse_assignment();
        return n;
    }
    if (js_check(TOK_PLUS_ASSIGN) || js_check(TOK_MINUS_ASSIGN)) {
        int is_plus = js_check(TOK_PLUS_ASSIGN);
        js_advance();
        AstNode* n = js_alloc_ast(AST_ASSIGN_OP);
        str_copy(n->str_val, is_plus ? "+" : "-", sizeof(n->str_val));
        n->children[n->child_count++] = left;
        n->children[n->child_count++] = js_parse_assignment();
        return n;
    }
    return left;
}

AstNode* js_parse_expression(void) { return js_parse_assignment(); }
// ============================================================================
// 12. JAVASCRIPT ENGINE — PARSER (příkazy)
// ============================================================================
AstNode* js_parse_block(void) {
    AstNode* block = js_alloc_ast(AST_BLOCK);
    if (js_check(TOK_LBRACE)) {
        js_advance();
        while (!js_check(TOK_RBRACE) && !js_check(TOK_EOF) && block->child_count < MAX_AST_CHILDREN) {
            block->children[block->child_count++] = js_parse_statement();
        }
        js_expect(TOK_RBRACE);
    } else {
        // jednořádkový příkaz bez složených závorek (if (x) foo(); )
        if (block->child_count < MAX_AST_CHILDREN) {
            block->children[block->child_count++] = js_parse_statement();
        }
    }
    return block;
}

AstNode* js_parse_var_decl(void) {
    js_advance(); // var/let/const
    AstNode* decl = js_alloc_ast(AST_VAR_DECL);
    if (js_check(TOK_IDENT)) {
        str_copy(decl->str_val, js_peek()->text, sizeof(decl->str_val));
        js_advance();
    }
    if (js_match(TOK_ASSIGN)) {
        decl->children[decl->child_count++] = js_parse_assignment();
    }
    js_match(TOK_SEMI);
    return decl;
}

AstNode* js_parse_if(void) {
    js_advance(); // 'if'
    js_expect(TOK_LPAREN);
    AstNode* n = js_alloc_ast(AST_IF);
    n->children[n->child_count++] = js_parse_expression(); // cond
    js_expect(TOK_RPAREN);
    n->children[n->child_count++] = js_parse_block();       // then
    if (js_check(TOK_KW_ELSE)) {
        js_advance();
        if (js_check(TOK_KW_IF)) {
            n->children[n->child_count++] = js_parse_if(); // else if
        } else {
            n->children[n->child_count++] = js_parse_block(); // else
        }
    }
    return n;
}

AstNode* js_parse_for(void) {
    js_advance(); // 'for'
    js_expect(TOK_LPAREN);
    AstNode* n = js_alloc_ast(AST_FOR);

    // init
    if (js_check(TOK_KW_VAR) || js_check(TOK_KW_LET) || js_check(TOK_KW_CONST)) {
        n->children[n->child_count++] = js_parse_var_decl(); // ';'
    } else if (!js_check(TOK_SEMI)) {
        n->children[n->child_count++] = js_parse_expression();
        js_match(TOK_SEMI);
    } else {
        n->children[n->child_count++] = js_alloc_ast(AST_UNDEF);
        js_match(TOK_SEMI);
    }

    // condition
    if (!js_check(TOK_SEMI)) n->children[n->child_count++] = js_parse_expression();
    else n->children[n->child_count++] = js_alloc_ast(AST_BOOL); // chybějící podmínka = true (nekonečný for(;;))
    js_match(TOK_SEMI);

    // update
    if (!js_check(TOK_RPAREN)) n->children[n->child_count++] = js_parse_expression();
    else n->children[n->child_count++] = js_alloc_ast(AST_UNDEF);
    js_expect(TOK_RPAREN);

    // tělo
    n->children[n->child_count++] = js_parse_block();
    return n;
}

AstNode* js_parse_while(void) {
    js_advance(); // 'while'
    js_expect(TOK_LPAREN);
    AstNode* n = js_alloc_ast(AST_WHILE);
    n->children[n->child_count++] = js_parse_expression();
    js_expect(TOK_RPAREN);
    n->children[n->child_count++] = js_parse_block();
    return n;
}

AstNode* js_parse_function_decl(void) {
    js_advance(); // 'function'
    AstNode* fn = js_alloc_ast(AST_FUNC_DECL);
    if (js_check(TOK_IDENT)) { str_copy(fn->str_val, js_peek()->text, sizeof(fn->str_val)); js_advance(); }
    js_expect(TOK_LPAREN);
    while (!js_check(TOK_RPAREN) && !js_check(TOK_EOF)) {
        if (js_check(TOK_IDENT) && fn->param_count < 8) {
            str_copy(fn->params[fn->param_count++], js_peek()->text, 32);
        }
        js_advance();
        js_match(TOK_COMMA);
    }
    js_expect(TOK_RPAREN);
    fn->children[fn->child_count++] = js_parse_block();
    return fn;
}

AstNode* js_parse_statement(void) {
    if (js_check(TOK_KW_VAR) || js_check(TOK_KW_LET) || js_check(TOK_KW_CONST)) return js_parse_var_decl();
    if (js_check(TOK_KW_IF)) return js_parse_if();
    if (js_check(TOK_KW_FOR)) return js_parse_for();
    if (js_check(TOK_KW_WHILE)) return js_parse_while();
    if (js_check(TOK_KW_FUNCTION)) return js_parse_function_decl();
    if (js_check(TOK_LBRACE)) return js_parse_block();
    if (js_check(TOK_KW_RETURN)) {
        js_advance();
        AstNode* n = js_alloc_ast(AST_RETURN);
        if (!js_check(TOK_SEMI) && !js_check(TOK_RBRACE) && !js_check(TOK_EOF)) {
            n->children[n->child_count++] = js_parse_expression();
        }
        js_match(TOK_SEMI);
        return n;
    }
    if (js_check(TOK_SEMI)) { js_advance(); return js_alloc_ast(AST_UNDEF); } // prázdný příkaz

    // expression statement
    AstNode* expr = js_parse_expression();
    js_match(TOK_SEMI);
    AstNode* stmt = js_alloc_ast(AST_EXPR_STMT);
    stmt->children[stmt->child_count++] = expr;
    return stmt;
}


AstNode* js_parse_program(void) {
    js_parse_pos = 0;
    AstNode* program = js_alloc_ast(AST_PROGRAM);
    while (!js_check(TOK_EOF) && program->child_count < MAX_AST_CHILDREN) {
        program->children[program->child_count++] = js_parse_statement();
    }
    return program;
}



void js_reset_engine(void) {
    js_global_count = 0;
    js_local_count = 0;
    js_in_function = 0;
    js_func_count = 0;
    js_returning = 0;
    js_string_pool_used = 0;
    js_ast_count = 0;
}

JSVar* js_find_local(const char* name) {
    if (!js_in_function) return 0;
    for (int i = 0; i < js_local_count; i++) {
        if (str_eq(js_locals[i].name, name)) return &js_locals[i];
    }
    return 0;
}
JSVar* js_find_global(const char* name) {
    for (int i = 0; i < js_global_count; i++) {
        if (str_eq(js_globals[i].name, name)) return &js_globals[i];
    }
    return 0;
}

JSValue js_get_var(const char* name) {
    JSVar* v = js_find_local(name);
    if (v) return v->value;
    v = js_find_global(name);
    if (v) return v->value;
    return js_undefined();
}


void js_set_var(const char* name, JSValue value) {
    JSVar* v = js_find_local(name);
    if (v) { v->value = value; return; }
    v = js_find_global(name);
    if (v) { v->value = value; return; }
    if (js_in_function && js_local_count < MAX_LOCALS) {
        str_copy(js_locals[js_local_count].name, name, 32);
        js_locals[js_local_count].value = value;
        js_local_count++;
        return;
    }
    if (js_global_count < MAX_GLOBALS) {
        str_copy(js_globals[js_global_count].name, name, 32);
        js_globals[js_global_count].value = value;
        js_global_count++;
    }
}

// 'var x;' / 'var x = ...;' deklaruje vždy do AKTUÁLNÍHO scope (lokální
// uvnitř funkce, jinak globální) 
void js_declare_var(const char* name, JSValue value) {
    if (js_in_function) {
        JSVar* v = js_find_local(name);
        if (!v && js_local_count < MAX_LOCALS) {
            str_copy(js_locals[js_local_count].name, name, 32);
            v = &js_locals[js_local_count];
            js_local_count++;
        }
        if (v) v->value = value;
        return;
    }
    JSVar* v = js_find_global(name);
    if (!v && js_global_count < MAX_GLOBALS) {
        str_copy(js_globals[js_global_count].name, name, 32);
        v = &js_globals[js_global_count];
        js_global_count++;
    }
    if (v) v->value = value;
}

void js_register_function(AstNode* decl) {
    if (js_func_count >= MAX_JS_FUNCS) return;
    str_copy(js_functions[js_func_count].name, decl->str_val, 32);
    js_functions[js_func_count].decl = decl;
    js_func_count++;
}
AstNode* js_find_function(const char* name) {
    for (int i = 0; i < js_func_count; i++) {
        if (str_eq(js_functions[i].name, name)) return js_functions[i].decl;
    }
    return 0;
}

JSValue js_eval(AstNode* node);
void js_exec_stmt(AstNode* node);
void js_exec_block(AstNode* block);

// --- volání funkce (uživatelské i nativní DOM/console funkce řeší js_call_builtin) ---
JSValue js_call_builtin(const char* name, AstNode* call_node, int* handled);
JSValue js_call_member_builtin(JSValue obj, const char* member, AstNode* call_node, int* handled);

JSValue js_call_user_function(AstNode* fn_decl, AstNode* call_node) {
 
    JSValue args[8];
    int arg_count = call_node->child_count - 1; // children[0] = callee
    if (arg_count > 8) arg_count = 8;
    for (int i = 0; i < arg_count; i++) {
        args[i] = js_eval(call_node->children[i + 1]);
    }

    JSVar saved_locals[MAX_LOCALS];
    int saved_local_count = js_local_count;
    int saved_in_function = js_in_function;
    for (int i = 0; i < saved_local_count; i++) saved_locals[i] = js_locals[i];

    js_local_count = 0;
    js_in_function = 1;
    for (int i = 0; i < fn_decl->param_count && i < arg_count; i++) {
        js_declare_var(fn_decl->params[i], args[i]);
    }
    // parametry bez odpovídajícího argumentu = undefined
    for (int i = arg_count; i < fn_decl->param_count; i++) {
        js_declare_var(fn_decl->params[i], js_undefined());
    }

    js_returning = 0;
    js_exec_block(fn_decl->children[0]);

    JSValue result = js_returning ? js_return_value : js_undefined();
    js_returning = 0;

    // obnovíme scope volajícího
    js_local_count = saved_local_count;
    js_in_function = saved_in_function;
    for (int i = 0; i < saved_local_count; i++) js_locals[i] = saved_locals[i];

    return result;
}

JSValue js_eval_call(AstNode* node) {
    AstNode* callee = node->children[0];

    // foo(...)  
    if (callee->type == AST_IDENT) {
        AstNode* fn_decl = js_find_function(callee->str_val);
        if (fn_decl) return js_call_user_function(fn_decl, node);

        int handled = 0;
        JSValue r = js_call_builtin(callee->str_val, node, &handled);
        if (handled) return r;
        return js_undefined();
    }

    // obj.method(...)  - DOM/console/document bridge
    if (callee->type == AST_MEMBER) {
        JSValue obj = js_eval(callee->children[0]);
        int handled = 0;
        JSValue r = js_call_member_builtin(obj, callee->str_val, node, &handled);
        if (handled) return r;
        return js_undefined();
    }

    return js_undefined();
}


JSValue js_eval_assign_target_member(AstNode* member, JSValue rhs);

JSValue js_eval(AstNode* node) {
    if (!node) return js_undefined();

    switch (node->type) {
        case AST_NUM: return js_num_val(node->num_val);
        case AST_STR: return js_str_val(js_alloc_string_z(node->str_val));
        case AST_BOOL: return js_bool_val((int)node->num_val);
        case AST_NULL: return js_null_val();
        case AST_UNDEF: return js_undefined();
        case AST_IDENT: {
            // "document" a "console" jsou pseudo-objekty bez vlastní proměnné 
            if (str_eq(node->str_val, "document")) return js_str_val("document");
            if (str_eq(node->str_val, "console")) return js_str_val("console");

            // Pokud jméno odpovídá deklarované funkci (function fn() {...})
            
            if (!js_find_local(node->str_val) && !js_find_global(node->str_val)) {
                if (js_find_function(node->str_val)) {
                    return js_str_val(js_alloc_string_z(node->str_val));
                }
            }
            return js_get_var(node->str_val);
        }

        case AST_UNOP: {
            if (str_eq(node->str_val, "!")) return js_bool_val(!js_truthy(js_eval(node->children[0])));
            if (str_eq(node->str_val, "-")) return js_num_val(-js_to_number(js_eval(node->children[0])));
            if (str_eq(node->str_val, "+")) return js_num_val(js_to_number(js_eval(node->children[0])));
            if (str_eq(node->str_val, "++pre") || str_eq(node->str_val, "--pre")) {
                AstNode* target = node->children[0];
                double cur = js_to_number(js_eval(target));
                double next = cur + (str_eq(node->str_val, "++pre") ? 1 : -1);
                if (target->type == AST_IDENT) js_set_var(target->str_val, js_num_val(next));
                return js_num_val(next);
            }
            return js_undefined();
        }

        case AST_POSTFIX: {
            AstNode* target = node->children[0];
            double cur = js_to_number(js_eval(target));
            double next = cur + (str_eq(node->str_val, "++") ? 1 : -1);
            if (target->type == AST_IDENT) js_set_var(target->str_val, js_num_val(next));
            return js_num_val(cur); // postfix vrací PŮVODNÍ hodnotu
        }

        case AST_BINOP: {
            JSValue l = js_eval(node->children[0]);
            JSValue r = js_eval(node->children[1]);
            const char* op = node->str_val;

            if (str_eq(op, "+")) {
                if (l.type == JSV_STRING || r.type == JSV_STRING) {
                    char lb[64], rb[64];
                    js_to_string(l, lb, sizeof(lb));
                    js_to_string(r, rb, sizeof(rb));
                    char combined[128];
                    int li = str_len(lb), ri = str_len(rb);
                    if (li > 127) li = 127;
                    for (int i = 0; i < li; i++) combined[i] = lb[i];
                    int total = li;
                    for (int i = 0; i < ri && total < 127; i++) combined[total++] = rb[i];
                    combined[total] = '\0';
                    return js_str_val(js_alloc_string_z(combined));
                }
                return js_num_val(js_to_number(l) + js_to_number(r));
            }
            if (str_eq(op, "-")) return js_num_val(js_to_number(l) - js_to_number(r));
            if (str_eq(op, "*")) return js_num_val(js_to_number(l) * js_to_number(r));
            if (str_eq(op, "/")) {
                double rn = js_to_number(r);
                return js_num_val(rn != 0 ? js_to_number(l) / rn : 0);
            }
            if (str_eq(op, "%")) {
                long rn = (long)js_to_number(r);
                long ln = (long)js_to_number(l);
                return js_num_val(rn != 0 ? (double)(ln % rn) : 0);
            }
            if (str_eq(op, "<"))  return js_bool_val(js_to_number(l) < js_to_number(r));
            if (str_eq(op, ">"))  return js_bool_val(js_to_number(l) > js_to_number(r));
            if (str_eq(op, "<=")) return js_bool_val(js_to_number(l) <= js_to_number(r));
            if (str_eq(op, ">=")) return js_bool_val(js_to_number(l) >= js_to_number(r));
            if (str_eq(op, "==") || str_eq(op, "!=")) {
                int eq;
                if (l.type == JSV_STRING && r.type == JSV_STRING) eq = str_eq(l.str, r.str);
                else if (l.type == JSV_DOMNODE || r.type == JSV_DOMNODE) eq = (l.dom == r.dom);
                else eq = js_to_number(l) == js_to_number(r);
                return js_bool_val(str_eq(op, "==") ? eq : !eq);
            }
            return js_undefined();
        }

        case AST_LOGICAL: {
            JSValue l = js_eval(node->children[0]);
            if (str_eq(node->str_val, "&&")) return js_truthy(l) ? js_eval(node->children[1]) : l;
            return js_truthy(l) ? l : js_eval(node->children[1]); // ||
        }

        case AST_ASSIGN: {
            AstNode* target = node->children[0];
            JSValue rhs = js_eval(node->children[1]);
            if (target->type == AST_IDENT) { js_set_var(target->str_val, rhs); return rhs; }
            if (target->type == AST_MEMBER) { return js_eval_assign_target_member(target, rhs); }
            return rhs;
        }

        case AST_ASSIGN_OP: {
            AstNode* target = node->children[0];
            JSValue rhs = js_eval(node->children[1]);
            JSValue cur = js_eval(target);
            JSValue result;
            if (str_eq(node->str_val, "+")) {
                if (cur.type == JSV_STRING || rhs.type == JSV_STRING) {
                    char lb[64], rb[64];
                    js_to_string(cur, lb, sizeof(lb));
                    js_to_string(rhs, rb, sizeof(rb));
                    char combined[128];
                    int li = str_len(lb), ri = str_len(rb);
                    if (li > 127) li = 127;
                    for (int i = 0; i < li; i++) combined[i] = lb[i];
                    int total = li;
                    for (int i = 0; i < ri && total < 127; i++) combined[total++] = rb[i];
                    combined[total] = '\0';
                    result = js_str_val(js_alloc_string_z(combined));
                } else {
                    result = js_num_val(js_to_number(cur) + js_to_number(rhs));
                }
            } else {
                result = js_num_val(js_to_number(cur) - js_to_number(rhs));
            }
            if (target->type == AST_IDENT) js_set_var(target->str_val, result);
            return result;
        }

        case AST_CALL: return js_eval_call(node);

        case AST_MEMBER: {
            JSValue obj = js_eval(node->children[0]);
            int handled = 0;
            JSValue r = js_call_member_builtin(obj, node->str_val, 0 /* no call, jen get */, &handled);
            if (handled) return r;
            return js_undefined();
        }

        case AST_FUNC_EXPR: {
            // function expression jako hodnota (pro addEventListener(fn)) -
            // protože nemáme closures/heap objekty, "hodnotou" funkce je
            // jednoduše její AST uzel uložený jako anonymní funkce v tabulce.
            static int anon_counter = 0;
            char anon_name[32];
            anon_name[0] = '_'; anon_name[1] = 'a'; anon_name[2] = 'n';
            int n = anon_counter++;
            int i = 3;
            if (n == 0) anon_name[i++] = '0';
            else { char tmp[8]; int ti=0; while(n>0){tmp[ti++]='0'+(n%10); n/=10;} while(ti>0) anon_name[i++]=tmp[--ti]; }
            anon_name[i] = '\0';
            str_copy(node->str_val, anon_name, sizeof(node->str_val));
            js_register_function(node);
            return js_str_val(js_alloc_string_z(anon_name)); // "ukazatel" na funkci = její jméno ve funkční tabulce
        }

        default: return js_undefined();
    }
}
// ============================================================================
// 14. JAVASCRIPT ENGINE — EXEKUCE PŘÍKAZŮ
// ============================================================================
void js_exec_block(AstNode* block) {
    for (int i = 0; i < block->child_count && !js_returning; i++) {
        js_exec_stmt(block->children[i]);
    }
}

void js_exec_stmt(AstNode* node) {
    if (!node || js_returning) return;

    switch (node->type) {
        case AST_VAR_DECL: {
            JSValue val = (node->child_count > 0) ? js_eval(node->children[0]) : js_undefined();
            js_declare_var(node->str_val, val);
            return;
        }
        case AST_EXPR_STMT: js_eval(node->children[0]); return;
        case AST_BLOCK: js_exec_block(node); return;
        case AST_FUNC_DECL: js_register_function(node); return;

        case AST_IF: {
            JSValue cond = js_eval(node->children[0]);
            if (js_truthy(cond)) {
                js_exec_block(node->children[1]);
            } else if (node->child_count > 2) {
                AstNode* alt = node->children[2];
                if (alt->type == AST_IF) js_exec_stmt(alt);
                else js_exec_block(alt);
            }
            return;
        }

        case AST_WHILE: {
            int guard = 0;
            while (js_truthy(js_eval(node->children[0])) && !js_returning) {
                js_exec_block(node->children[1]);
                guard++;
                if (guard > 100000) break; // bezpečnostní pojistka proti zacyklení
            }
            return;
        }

        case AST_FOR: {
            // children: [0]=init, [1]=cond, [2]=update, [3]=body
            if (node->children[0]->type == AST_VAR_DECL) js_exec_stmt(node->children[0]);
            else js_eval(node->children[0]);

            int guard = 0;
            while (js_truthy(js_eval(node->children[1])) && !js_returning) {
                js_exec_block(node->children[3]);
                js_eval(node->children[2]);
                guard++;
                if (guard > 100000) break; // bezpečnostní pojistka
            }
            return;
        }

        case AST_RETURN: {
            js_return_value = (node->child_count > 0) ? js_eval(node->children[0]) : js_undefined();
            js_returning = 1;
            return;
        }

        default: js_eval(node); return; // výrazy použité jako příkaz (fallback)
    }
}
// ============================================================================
// 15. JAVASCRIPT ENGINE — DOM BRIDGE
// ============================================================================

DOMNode* dom_find_at_point(DOMNode* node, int doc_x, int doc_y) {
    if (!node) return 0;
    if (node->style.is_inline == -1) return 0; // display:none nelze kliknout

    for (int i = node->child_count - 1; i >= 0; i--) {
        DOMNode* found = dom_find_at_point(node->children[i], doc_x, doc_y);
        if (found) return found;
    }

    if (doc_x >= node->render_x && doc_x < (node->render_x + node->render_w) &&
        doc_y >= node->render_y && doc_y < (node->render_y + node->render_h)) {
        return node;
    }
    return 0;
}


void url_encode(const char* src, char* out, int out_size) {
    static const char hex_digits[] = "0123456789ABCDEF";
    int pos = 0;
    for (int i = 0; src[i] && pos < out_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        int is_unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                             c == '.' || c == '~';
        if (is_unreserved) {
            out[pos++] = (char)c;
        } else if (c == ' ') {
            out[pos++] = '+';
        } else {
            if (pos + 3 > out_size - 1) break; // "%XX" se nevejde celé, radši nic dalšího nezapisovat
            out[pos++] = '%';
            out[pos++] = hex_digits[(c >> 4) & 0xF];
            out[pos++] = hex_digits[c & 0xF];
        }
    }
    out[pos] = '\0';
}


int dom_collect_ancestor_chain(DOMNode* root, DOMNode* target, DOMNode** out_chain, int max_len);


DOMNode* find_enclosing_form(DOMNode* page_root, DOMNode* target) {
    DOMNode* chain[64];
    int chain_len = dom_collect_ancestor_chain(page_root, target, chain, 64);
    for (int i = chain_len - 1; i >= 0; i--) {
        if (chain[i]->type == NODE_FORM) return chain[i];
    }
    return 0;
}

DOMNode* dom_find_by_id(DOMNode* node, const char* id) {
    if (!node) return 0;
    if (node->id_name[0] && str_eq(node->id_name, id)) return node;
    for (int i = 0; i < node->child_count; i++) {
        DOMNode* found = dom_find_by_id(node->children[i], id);
        if (found) return found;
    }
    return 0;
}

// Najde první element s danou třídou (DFS, stejný vzor jako dom_find_by_id).

DOMNode* dom_find_by_class(DOMNode* node, const char* class_name) {
    if (!node) return 0;
    // Změněno z str_eq na has_class
    if (node->class_name[0] && has_class(node->class_name, class_name)) return node;
    for (int i = 0; i < node->child_count; i++) {
        DOMNode* found = dom_find_by_class(node->children[i], class_name);
        if (found) return found;
    }
    return 0;
}
// Najde první element podle jména tagu (DFS) - pro querySelector('div') apod.
DOMNode* dom_find_by_tag(DOMNode* node, const char* tag_name) {
    if (!node) return 0;
    if (node->tag_name[0] && str_eq(node->tag_name, tag_name)) return node;
    for (int i = 0; i < node->child_count; i++) {
        DOMNode* found = dom_find_by_tag(node->children[i], tag_name);
        if (found) return found;
    }
    return 0;
}

// Nahradí veškerý obsah elementu jediným textovým uzlem s daným textem.
// Používá se pro innerHTML i textContent 
void dom_set_text_content(DOMNode* node, const char* text) {
    node->child_count = 0;
    DOMNode* t = alloc_node();
    if (!t) return;
    t->type = NODE_TEXT;
    int len = str_len(text);
    if (len > 127) len = 127;
    for (int i = 0; i < len; i++) t->text[i] = text[i];
    t->text[len] = '\0';
    t->text_len = len;
    t->style.has_color = 1; t->style.text_color = node->style.has_color ? node->style.text_color : 0xFF000000;
    t->style.has_font_size = 1; t->style.font_scale = node->style.font_scale ? node->style.font_scale : 1;
    t->style.has_text_align = 1; t->style.text_align = node->style.text_align;
    node->children[node->child_count++] = t;
    layout_dirty = 1; 
}

// Vrátí "souvislý" textový obsah elementu 
void dom_get_text_content(DOMNode* node, char* out, int out_size) {
    out[0] = '\0';
    int pos = 0;
    for (int i = 0; i < node->child_count && pos < out_size - 1; i++) {
        DOMNode* c = node->children[i];
        if (c->type == NODE_TEXT) {
            for (int j = 0; j < c->text_len && pos < out_size - 1; j++) out[pos++] = c->text[j];
        }
    }
    out[pos] = '\0';
}

// --- onclick handlery ---
// Protože nemáme closures/heap objekty, "handler" je prostě jméno funkce
// (buď deklarované, nebo anonymní z FUNC_EXPR registrované do js_functions).
// Uložíme dvojici (DOMNode*, jméno funkce) do tabulky a zkontrolujeme ji
// v hlavní smyčce main() při kliknutí myší.
#define MAX_CLICK_HANDLERS 64
typedef struct { DOMNode* node; char fn_name[32]; } ClickHandler;
ClickHandler js_click_handlers[MAX_CLICK_HANDLERS];
int js_click_handler_count = 0;

void js_register_click_handler(DOMNode* node, const char* fn_name) {
    for (int i = 0; i < js_click_handler_count; i++) {
        if (js_click_handlers[i].node == node) {
            str_copy(js_click_handlers[i].fn_name, fn_name, 32);
            return;
        }
    }
    if (js_click_handler_count < MAX_CLICK_HANDLERS) {
        js_click_handlers[js_click_handler_count].node = node;
        str_copy(js_click_handlers[js_click_handler_count].fn_name, fn_name, 32);
        js_click_handler_count++;
    }
}
// Zavolá onclick handler uzlu, pokud nějaký má.
int js_fire_click(DOMNode* node) {
    for (int i = 0; i < js_click_handler_count; i++) {
        if (js_click_handlers[i].node == node) {
            AstNode* fn = js_find_function(js_click_handlers[i].fn_name);
            if (fn) {
                AstNode fake_call; fake_call.type = AST_CALL; fake_call.child_count = 1;
                fake_call.children[0] = fn; 
                js_call_user_function(fn, &fake_call);
            }
            return 1;
        }
    }
    return 0;
}

// Najde nejbližšího předka uzlu 'target' uvnitř podstromu 'root' 
int dom_collect_ancestor_chain(DOMNode* root, DOMNode* target, DOMNode** out_chain, int max_len) {
    if (!root) return 0;
    if (root == target) {
        if (max_len > 0) out_chain[0] = root;
        return 1;
    }
    for (int i = 0; i < root->child_count; i++) {
        int n = dom_collect_ancestor_chain(root->children[i], target, out_chain + 1, max_len - 1);
        if (n > 0) {
            if (max_len > 0) out_chain[0] = root;
            return n + 1;
        }
    }
    return 0;
}


int js_fire_click_bubbling(DOMNode* page_root, DOMNode* node) {
    DOMNode* chain[64];
    int chain_len = dom_collect_ancestor_chain(page_root, node, chain, 64);
    // chain[0] = root, chain[chain_len-1] 
    for (int i = chain_len - 1; i >= 0; i--) {
        if (js_fire_click(chain[i])) return 1;
    }
    return 0;
}


const char* js_value_as_func_name(JSValue v) {
    if (v.type == JSV_STRING && js_find_function(v.str)) return v.str;
    return 0;
}

// --- console.log(...) / alert(...) 
DOMNode* js_console_root = 0; // nastaví navigate_to/build_dom_tree na root_node

void js_console_log_to_page(const char* prefix, const char* text) {
    if (!js_console_root) return;
    DOMNode* line = alloc_node();
    if (!line) return;
    line->type = NODE_P;
    str_copy(line->tag_name, "p", sizeof(line->tag_name));
    line->style.has_color = 1; line->style.text_color = 0xFF888888;
    line->style.has_font_size = 1; line->style.font_scale = 1;
    line->style.has_margin = 1; line->style.margin = 2;
    line->style.has_padding = 1; line->style.padding = 0;
    line->style.has_display = 1; line->style.is_inline = 0;
    line->style.has_text_align = 1; line->style.text_align = 0;
    line->style.has_border = 1; line->style.border_w = 0;

    char combined[160];
    int pi = 0;
    while (prefix[pi] && pi < 30) { combined[pi] = prefix[pi]; pi++; }
    int ti = 0;
    while (text[ti] && pi < 159) { combined[pi++] = text[ti++]; }
    combined[pi] = '\0';
    dom_set_text_content(line, combined);

    if (js_console_root->child_count < MAX_CHILDREN) {
        js_console_root->children[js_console_root->child_count++] = line;
    }
}

// --- volání globálních funkcí jako console.log/alert/document.getElementById ---

int js_timers_tick(void) {
    long now = sys_get_time_ms();
    int fired = 0;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!js_timers[i].active) continue;
        if (now < js_timers[i].deadline_ms) continue;

        AstNode* fn = js_find_function(js_timers[i].fn_name);
        if (fn) {
            AstNode fake_call;
            fake_call.type = AST_CALL;
            fake_call.child_count = 1;
            fake_call.children[0] = fn;
            js_call_user_function(fn, &fake_call);
            fired = 1;
        }

        if (js_timers[i].interval_ms > 0) {
           
            js_timers[i].deadline_ms += js_timers[i].interval_ms;
        } else {
            js_timers[i].active = 0; // setTimeout: jednorázový
        }
    }
    return fired;
}

JSValue js_call_builtin(const char* name, AstNode* call_node, int* handled) {
    *handled = 1;

    if (str_eq(name, "alert")) {
        if (call_node->child_count > 1) {
            JSValue v = js_eval(call_node->children[1]);
            char buf[80]; js_to_string(v, buf, sizeof(buf));
            js_console_log_to_page("[alert] ", buf);
        }
        return js_undefined();
    }
// ---  fetch("url") ---
if (str_eq(name, "fetch")) {
    if (call_node->child_count < 2) { *handled = 0; return js_undefined(); }
    
    //  URL z argumentu
    JSValue url_val = js_eval(call_node->children[1]);
    char rel_url[128]; 
    js_to_string(url_val, rel_url, sizeof(rel_url));
    
    // Z URL v JS (např. "/data.txt")  plnou absolutní URL
    char full_url[256];
    resolve_relative_url(rel_url, full_url, sizeof(full_url));
    
    // Rozparsujeme pro síťový syscall
    char domain[128], path[128];
    int is_https = 1, port = 0, is_ip = 0;
    parse_url(full_url, domain, path, &is_https, &port, &is_ip);
    
    // Ušetříme DNS dotaz, pokud se ptáme stejného serveru
    uint32_t target_ip = str_eq(domain, current_domain) ? current_server_ip : 
                         (is_ip ? parse_ipv4(domain) : sys_dns_resolve(domain));
    if (!port) port = is_https ? 443 : 80;
    
    // Stáhneme data do dočasného bufferu (např. max 4 KB)
    static char fetch_buf[4096];
    for (int i = 0; i < 4096; i++) fetch_buf[i] = 0;
    
    int bytes = is_https ? 
        sys_https_get(target_ip, domain, port, path, fetch_buf, 4096) :
        sys_http_get(target_ip, port, path, fetch_buf, 4096);
        
    if (bytes > 0) {
        // Úspěch! Alokujeme stažený text do JS paměti a vrátíme ho jako String
        return js_str_val(js_alloc_string(fetch_buf, bytes));
    } else {
        // Selhání (404 nebo chyba sítě)
        return js_null_val();
    }
}
    // setTimeout(fn_name_string, delay_ms) nebo setTimeout(function_ref, delay_ms)
    
    if (str_eq(name, "setTimeout") || str_eq(name, "setInterval")) {
        if (call_node->child_count < 2) { *handled = 0; return js_undefined(); }

        char fn_name[64] = {0};
        AstNode* fn_arg = call_node->children[1];
        if (fn_arg->type == AST_STR) {
            // setTimeout("jmeno", ms) - jméno jako string literal
            str_copy(fn_name, fn_arg->str_val, sizeof(fn_name));
        } else if (fn_arg->type == AST_IDENT) {
            // setTimeout(jmeno, ms) - identifikátor
            str_copy(fn_name, fn_arg->str_val, sizeof(fn_name));
        }
        // Ostatní typy (lambda, výraz...) - fn_name zůstane prázdné -> no-op

        long delay_ms = 0;
        if (call_node->child_count > 2) {
            JSValue delay_val = js_eval(call_node->children[2]);
            delay_ms = (long)delay_val.num;
        }
        if (delay_ms < 0) delay_ms = 0;

        long interval_ms = str_eq(name, "setInterval") ? delay_ms : 0;
        int id = (fn_name[0]) ? js_timer_register(fn_name, delay_ms, interval_ms) : js_timer_next_id++;
        return js_num_val((double)id);
    }

    if (str_eq(name, "clearTimeout") || str_eq(name, "clearInterval")) {
        if (call_node->child_count > 1) {
            JSValue id_val = js_eval(call_node->children[1]);
            js_timer_clear((int)id_val.num);
        }
        return js_undefined();
    }

   
    *handled = 0;
    return js_undefined();
}

// --- volání/get member funkcí na objektech: console.X(), document.X(), element.X(), element.style.X ---
// call_node == 0 znamená "jen čtení vlastnosti" (ne volání) - používá se z AST_MEMBER.
JSValue js_call_member_builtin(JSValue obj, const char* member, AstNode* call_node, int* handled) {
    *handled = 1;

    // --- "document" pseudo-objekt: reprezentujeme ho jako string "document" ---
    if (obj.type == JSV_STRING && str_eq(obj.str, "document")) {
        if (str_eq(member, "getElementById") && call_node) {
            JSValue arg = js_eval(call_node->children[1]);
            char id[64]; js_to_string(arg, id, sizeof(id));
            DOMNode* found = dom_find_by_id(js_console_root, id);
            return found ? js_dom_val(found) : js_null_val();
        }
        if (str_eq(member, "querySelector") && call_node) {
            // Podpora jen základních selektorů: '#id', '.class', 'tagname'
            
            JSValue arg = js_eval(call_node->children[1]);
            char sel[64]; js_to_string(arg, sel, sizeof(sel));
            DOMNode* found = 0;
            if (sel[0] == '#') found = dom_find_by_id(js_console_root, sel + 1);
            else if (sel[0] == '.') found = dom_find_by_class(js_console_root, sel + 1);
            else found = dom_find_by_tag(js_console_root, sel);
            return found ? js_dom_val(found) : js_null_val();
        }
        if (str_eq(member, "cookie") && !call_node) {
            // document.cookie ČTENÍ (ne volání - proto !call_node, stejný
            
            static char cookie_buf[512];
            sys_cookie_get(current_domain, current_is_https, cookie_buf, sizeof(cookie_buf));
            return js_str_val(js_alloc_string_z(cookie_buf));
        }
        *handled = 0;
        return js_undefined();
    }

    // --- el.classList.add/remove/toggle/contains(...) ---
    // Rozpoznáno přes stejný trik jako document/console: classlist string
    // hodnota nese .dom ukazatel na element (viz "classList" větev výš).
    if (obj.type == JSV_STRING && str_eq(obj.str, "classlist")) {
        DOMNode* el = obj.dom;
        if (!el || !call_node) { *handled = 0; return js_undefined(); }
        JSValue arg = (call_node->child_count > 1) ? js_eval(call_node->children[1]) : js_undefined();
        char cls[64]; js_to_string(arg, cls, sizeof(cls));

        
        if (str_eq(member, "add")) {
            str_copy(el->class_name, cls, sizeof(el->class_name));
            compute_style(el, 0); // třída ovlivňuje CSS matchování -> přepočítat styl
            layout_dirty = 1;  
            return js_undefined();
        }
        if (str_eq(member, "remove")) {
            if (str_eq(el->class_name, cls)) el->class_name[0] = '\0';
            compute_style(el, 0);
            layout_dirty = 1;
            return js_undefined();
        }
        if (str_eq(member, "toggle")) {
            if (str_eq(el->class_name, cls)) el->class_name[0] = '\0';
            else str_copy(el->class_name, cls, sizeof(el->class_name));
            compute_style(el, 0);
            layout_dirty = 1;
            return js_undefined();
        }
        if (str_eq(member, "contains")) {
            return js_bool_val(el->class_name[0] && str_eq(el->class_name, cls));
        }
        *handled = 0;
        return js_undefined();
    }

    // --- "console" pseudo-objekt ---
    if (obj.type == JSV_STRING && str_eq(obj.str, "console")) {
        if (str_eq(member, "log") && call_node) {
            char buf[80]; buf[0] = '\0';
            int pos = 0;
            for (int i = 1; i < call_node->child_count; i++) {
                JSValue v = js_eval(call_node->children[i]);
                char piece[64]; js_to_string(v, piece, sizeof(piece));
                int pl = str_len(piece);
                if (i > 1 && pos < 78) buf[pos++] = ' ';
                for (int j = 0; j < pl && pos < 78; j++) buf[pos++] = piece[j];
            }
            buf[pos] = '\0';
            js_console_log_to_page("[log] ", buf);
            return js_undefined();
        }
        *handled = 0;
        return js_undefined();
    }

    // --- DOM element (JSV_DOMNODE) ---
    if (obj.type == JSV_DOMNODE) {
        DOMNode* el = obj.dom;
        if (!el) { *handled = 0; return js_undefined(); }

        if (str_eq(member, "innerHTML") || str_eq(member, "textContent")) {
            if (call_node) { *handled = 0; return js_undefined(); }
            char buf[128]; dom_get_text_content(el, buf, sizeof(buf));
            return js_str_val(js_alloc_string_z(buf));
        }

        if (str_eq(member, "style")) {
            // "el.style" samo o sobě (bez .X) vrátíme jako odkaz na element -
            // skutečné .style.color se řeší jako vnořený AST_MEMBER níž
         
            return obj;
        }

        if (str_eq(member, "classList")) {
            
            JSValue cl = js_str_val("classlist");
            cl.dom = el;
            return cl;
        }

        if (str_eq(member, "addEventListener") && call_node) {
            // addEventListener('click', fn)
            if (call_node->child_count >= 3) {
                JSValue evt = js_eval(call_node->children[1]);
                JSValue fn  = js_eval(call_node->children[2]);
                char evt_name[16]; js_to_string(evt, evt_name, sizeof(evt_name));
                const char* fn_name = js_value_as_func_name(fn);
                if (str_eq(evt_name, "click") && fn_name) {
                    js_register_click_handler(el, fn_name);
                }
            }
            return js_undefined();
        }

        if (str_eq(member, "getAttribute") && call_node) {
            JSValue arg = js_eval(call_node->children[1]);
            char attr[32]; js_to_string(arg, attr, sizeof(attr));
            if (str_eq(attr, "id")) return js_str_val(js_alloc_string_z(el->id_name));
            if (str_eq(attr, "class")) return js_str_val(js_alloc_string_z(el->class_name));
            if (str_eq(attr, "href")) return js_str_val(js_alloc_string_z(el->href));
            return js_null_val();
        }

        *handled = 0;
        return js_undefined();
    }

    *handled = 0;
    return js_undefined();
}

// Zpracuje přiřazení do member-výrazu: el.innerHTML = "...", el.style.color = "...",
// el.onclick = fn. target je AST_MEMBER uzel (target->children[0] je "obj" výraz,
// target->str_val je jméno property).
JSValue js_eval_assign_target_member(AstNode* target, JSValue rhs) {
    AstNode* obj_expr = target->children[0];
    const char* prop = target->str_val;

    // --- el.style.X = ...  -> obj_expr je samo AST_MEMBER "el.style" ---
    if (obj_expr->type == AST_MEMBER && str_eq(obj_expr->str_val, "style")) {
        JSValue el_val = js_eval(obj_expr->children[0]);
        if (el_val.type != JSV_DOMNODE || !el_val.dom) return rhs;
        DOMNode* el = el_val.dom;
        char valbuf[32]; js_to_string(rhs, valbuf, sizeof(valbuf));
        const char* vp = valbuf;

        if (str_eq(prop, "color")) { el->style.has_color = 1; el->style.text_color = parse_css_color(&vp); }
        else if (str_eq(prop, "backgroundColor") || str_eq(prop, "background")) { el->style.has_bg = 1; el->style.bg_color = parse_css_color(&vp); }
        else if (str_eq(prop, "display")) {
            el->style.has_display = 1;
            if (str_eq(valbuf, "none")) el->style.is_inline = -1;
            else if (str_eq(valbuf, "inline")) el->style.is_inline = 1;
            else el->style.is_inline = 0;
            layout_dirty = 1; 
        }
        else if (str_eq(prop, "width"))  { el->style.has_width = 1;  el->style.width  = parse_css_length(&vp); layout_dirty = 1; }
        else if (str_eq(prop, "height")) { el->style.has_height = 1; el->style.height = parse_css_length(&vp); layout_dirty = 1; }
        return rhs;
    }

    // --- el.innerHTML = "..." / el.textContent = "..." / document.cookie = "..." ---
    JSValue obj = js_eval(obj_expr);

    // document.cookie = "name=value; Secure" - document je JSV_STRING pseudo-
    // hodnota
    if (obj.type == JSV_STRING && str_eq(obj.str, "document")
        && str_eq(prop, "cookie")) {
        char cookie_str[256];
        js_to_string(rhs, cookie_str, sizeof(cookie_str));
        sys_cookie_set(current_domain, cookie_str);
        return rhs;
    }

    if (obj.type == JSV_DOMNODE && obj.dom) {
        if (str_eq(prop, "innerHTML") || str_eq(prop, "textContent")) {
            char buf[128]; js_to_string(rhs, buf, sizeof(buf));
            dom_set_text_content(obj.dom, buf);
            return rhs;
        }
        if (str_eq(prop, "onclick")) {
            const char* fn_name = js_value_as_func_name(rhs);
            if (fn_name) js_register_click_handler(obj.dom, fn_name);
            return rhs;
        }
    }
    return rhs;
}

// --- FUNKCE PRO NAČTENÍ STRÁNKY ---
// Spustí všechny nasbírané <script> bloky aktuální stránky (build_dom_tree
// je naplnil do js_script_blocks[]). Volá se TEPRVE po sestavení celého DOM,
// aby document.getElementById/addEventListener měly co najít.
void js_run_page_scripts(DOMNode* page_root) {
    js_reset_engine();
    js_console_root = page_root;
    js_click_handler_count = 0;

    for (int b = 0; b < js_script_block_count; b++) {
        js_tokenize(js_script_blocks[b]);
        AstNode* program = js_parse_program();

        // první průchod: zaregistruj všechny function deklarace, aby se
        // vzájemně mohly volat bez ohledu na pořadí v souboru (hoisting)
        for (int i = 0; i < program->child_count; i++) {
            if (program->children[i]->type == AST_FUNC_DECL) {
                js_register_function(program->children[i]);
            }
        }
        // druhý průchod: vykonej zbytek (var, výrazy, volání na top-levelu)
        js_returning = 0;
        for (int i = 0; i < program->child_count && !js_returning; i++) {
            if (program->children[i]->type != AST_FUNC_DECL) {
                js_exec_stmt(program->children[i]);
            }
        }
        js_returning = 0;
    }
}