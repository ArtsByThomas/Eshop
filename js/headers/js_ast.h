#ifndef JS_AST_H
#define JS_AST_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AST_NODES 4096
#define MAX_AST_CHILDREN 32
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

struct AstNode {
    AstType type;
    char str_val[64];     
    double num_val;
    AstNode* children[MAX_AST_CHILDREN];
    int child_count;
    char params[8][32];
    int param_count;
};
AstNode* js_parse_program(void);

extern AstNode js_ast_pool[MAX_AST_NODES];
extern int js_ast_count;
int js_tokenize(const char* src);

#ifdef __cplusplus
}
#endif

#endif /* JS_AST_H */