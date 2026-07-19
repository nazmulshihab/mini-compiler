/*
 * ast.h  –  Abstract Syntax Tree for MiniLang
 *
 * Defines every node type the parser can build, plus helper
 * constructor / print / free functions used by all later phases.
 */

#ifndef AST_H
#define AST_H

/* ------------------------------------------------------------------ */
/* Data types supported by MiniLang                                    */
/* ------------------------------------------------------------------ */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_UNKNOWN          /* used when type cannot be determined       */
} DataType;

/* ------------------------------------------------------------------ */
/* Node kinds                                                          */
/* ------------------------------------------------------------------ */
typedef enum {
    NODE_PROGRAM,         /* root: ordered list of top-level stmts     */
    NODE_BLOCK,           /* { stmt* }  – introduces a new scope       */
    NODE_DECL,            /* int x;  /  bool b;                        */
    NODE_ASSIGN,          /* x = expr;                                 */
    NODE_IF,              /* if (cond) block [else block]              */
    NODE_WHILE,           /* while (cond) block                        */
    NODE_PRINT,           /* print(expr);                              */
    NODE_BINOP,           /* expr op expr                              */
    NODE_VAR,             /* identifier reference                      */
    NODE_INT_CONST,       /* integer literal                           */
    NODE_BOOL_CONST       /* true / false                              */
} NodeType;

/* ------------------------------------------------------------------ */
/* AST node – a single struct covers every node kind.                  */
/* Fields that are not applicable to a particular kind are zero/NULL.  */
/* ------------------------------------------------------------------ */
typedef struct ASTNode {
    NodeType  type;
    DataType  dtype;      /* type annotated by semantic analysis       */
    int       line;       /* source line for error messages            */

    /* ---- leaf values ---- */
    int   ival;           /* INT_CONST value; BOOL_CONST: 0/1          */
    char *sval;           /* VAR/DECL name;  BINOP operator string     */
    DataType decl_type;   /* declared type for NODE_DECL               */

    /* ---- child pointers ---- */
    struct ASTNode *left;  /* BINOP left; IF/WHILE condition           */
    struct ASTNode *right; /* BINOP right; ASSIGN expression           */
    struct ASTNode *body;  /* IF then-branch; WHILE body               */
    struct ASTNode *els;   /* IF else-branch (may be NULL)             */

    /* ---- statement list (NODE_PROGRAM, NODE_BLOCK) ---- */
    struct ASTNode **stmts;
    int  stmt_count;
    int  stmt_capacity;
} ASTNode;

/* ------------------------------------------------------------------ */
/* Constructor helpers                                                  */
/* ------------------------------------------------------------------ */
ASTNode *ast_new_node     (NodeType type, int line);
ASTNode *ast_new_program  (int line);
ASTNode *ast_new_block    (int line);
ASTNode *ast_new_decl     (DataType dtype, const char *name, int line);
ASTNode *ast_new_assign   (const char *name, ASTNode *expr, int line);
ASTNode *ast_new_if       (ASTNode *cond, ASTNode *then_b,
                            ASTNode *else_b, int line);
ASTNode *ast_new_while    (ASTNode *cond, ASTNode *body, int line);
ASTNode *ast_new_print    (ASTNode *expr, int line);
ASTNode *ast_new_binop    (const char *op, ASTNode *left,
                            ASTNode *right, int line);
ASTNode *ast_new_var      (const char *name, int line);
ASTNode *ast_new_int_const (int val, int line);
ASTNode *ast_new_bool_const(int val, int line);

void ast_add_stmt (ASTNode *parent, ASTNode *stmt);
void ast_print    (ASTNode *node, int indent);
void ast_free     (ASTNode *node);

const char *datatype_to_str(DataType t);

#endif /* AST_H */
