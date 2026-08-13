#include "headers/js_ast.h"
#include "headers/js_lexer.h" // Potřebuje znát typy tokenů a přistupovat k js_tokens
#include "../str_utils.h"
// Externí string funkce
extern void str_copy(char* dest, const char* src, size_t n);

// --- FYZICKÁ ALOKACE PAMĚTI PRO AST ---
AstNode js_ast_pool[MAX_AST_NODES];
int js_ast_count = 0;
int js_parse_pos = 0;

// --- POMOCNÉ FUNKCE PARSERU ---
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
void js_expect(TokenType t) { if (js_check(t)) js_advance(); }

// Deklarace dopředu, kvůli rekurzi
AstNode* js_parse_expression(void);
AstNode* js_parse_assignment(void);
AstNode* js_parse_statement(void);
AstNode* js_parse_block(void);

// Zde vlož všechny funkce pro parsování:
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