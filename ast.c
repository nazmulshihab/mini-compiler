/*
 * ast.c  –  AST node allocation, construction, printing, and cleanup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ------------------------------------------------------------------ */
/* Internal utilities                                                   */
/* ------------------------------------------------------------------ */

static ASTNode *alloc_node(NodeType type, int line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    if (!n) { fprintf(stderr, "Out of memory\n"); exit(1); }
    n->type  = type;
    n->dtype = TYPE_UNKNOWN;
    n->line  = line;
    return n;
}

/* ------------------------------------------------------------------ */
/* Constructors                                                         */
/* ------------------------------------------------------------------ */

ASTNode *ast_new_node(NodeType type, int line)
{
    return alloc_node(type, line);
}

ASTNode *ast_new_program(int line)
{
    return alloc_node(NODE_PROGRAM, line);
}

ASTNode *ast_new_block(int line)
{
    return alloc_node(NODE_BLOCK, line);
}

ASTNode *ast_new_decl(DataType dtype, const char *name, int line)
{
    ASTNode *n    = alloc_node(NODE_DECL, line);
    n->decl_type  = dtype;
    n->sval       = strdup(name);
    return n;
}

ASTNode *ast_new_assign(const char *name, ASTNode *expr, int line)
{
    ASTNode *n = alloc_node(NODE_ASSIGN, line);
    n->sval    = strdup(name);
    n->right   = expr;
    return n;
}

ASTNode *ast_new_if(ASTNode *cond, ASTNode *then_b,
                     ASTNode *else_b, int line)
{
    ASTNode *n = alloc_node(NODE_IF, line);
    n->left    = cond;
    n->body    = then_b;
    n->els     = else_b;
    return n;
}

ASTNode *ast_new_while(ASTNode *cond, ASTNode *body, int line)
{
    ASTNode *n = alloc_node(NODE_WHILE, line);
    n->left    = cond;
    n->body    = body;
    return n;
}

ASTNode *ast_new_print(ASTNode *expr, int line)
{
    ASTNode *n = alloc_node(NODE_PRINT, line);
    n->left    = expr;
    return n;
}

ASTNode *ast_new_binop(const char *op, ASTNode *left,
                        ASTNode *right, int line)
{
    ASTNode *n = alloc_node(NODE_BINOP, line);
    n->sval    = strdup(op);
    n->left    = left;
    n->right   = right;
    return n;
}

ASTNode *ast_new_var(const char *name, int line)
{
    ASTNode *n = alloc_node(NODE_VAR, line);
    n->sval    = strdup(name);
    return n;
}

ASTNode *ast_new_int_const(int val, int line)
{
    ASTNode *n = alloc_node(NODE_INT_CONST, line);
    n->ival    = val;
    n->dtype   = TYPE_INT;
    return n;
}

ASTNode *ast_new_bool_const(int val, int line)
{
    ASTNode *n = alloc_node(NODE_BOOL_CONST, line);
    n->ival    = val;
    n->dtype   = TYPE_BOOL;
    return n;
}

/* ------------------------------------------------------------------ */
/* Statement list management (for PROGRAM / BLOCK)                     */
/* ------------------------------------------------------------------ */

void ast_add_stmt(ASTNode *parent, ASTNode *stmt)
{
    if (!parent || !stmt) return;
    if (parent->stmt_count >= parent->stmt_capacity) {
        parent->stmt_capacity = (parent->stmt_capacity == 0)
                                ? 8 : parent->stmt_capacity * 2;
        parent->stmts = realloc(parent->stmts,
                                parent->stmt_capacity * sizeof(ASTNode *));
        if (!parent->stmts) { fprintf(stderr, "Out of memory\n"); exit(1); }
    }
    parent->stmts[parent->stmt_count++] = stmt;
}

/* ------------------------------------------------------------------ */
/* Pretty printer                                                       */
/* ------------------------------------------------------------------ */

const char *datatype_to_str(DataType t)
{
    switch (t) {
        case TYPE_INT:     return "int";
        case TYPE_BOOL:    return "bool";
        case TYPE_VOID:    return "void";
        default:           return "unknown";
    }
}

static void indent(int d)
{
    for (int i = 0; i < d; i++) printf("  ");
}

void ast_print(ASTNode *node, int depth)
{
    if (!node) return;
    indent(depth);

    switch (node->type) {

        case NODE_PROGRAM:
            printf("PROGRAM\n");
            for (int i = 0; i < node->stmt_count; i++)
                ast_print(node->stmts[i], depth + 1);
            break;

        case NODE_BLOCK:
            printf("BLOCK\n");
            for (int i = 0; i < node->stmt_count; i++)
                ast_print(node->stmts[i], depth + 1);
            break;

        case NODE_DECL:
            printf("DECL  %-5s  %s\n",
                   datatype_to_str(node->decl_type), node->sval);
            break;

        case NODE_ASSIGN:
            printf("ASSIGN  %s =\n", node->sval);
            ast_print(node->right, depth + 1);
            break;

        case NODE_IF:
            printf("IF\n");
            indent(depth + 1); printf("COND:\n");
            ast_print(node->left, depth + 2);
            indent(depth + 1); printf("THEN:\n");
            ast_print(node->body, depth + 2);
            if (node->els) {
                indent(depth + 1); printf("ELSE:\n");
                ast_print(node->els, depth + 2);
            }
            break;

        case NODE_WHILE:
            printf("WHILE\n");
            indent(depth + 1); printf("COND:\n");
            ast_print(node->left, depth + 2);
            indent(depth + 1); printf("BODY:\n");
            ast_print(node->body, depth + 2);
            break;

        case NODE_PRINT:
            printf("PRINT\n");
            ast_print(node->left, depth + 1);
            break;

        case NODE_BINOP:
            printf("BINOP  '%s'\n", node->sval);
            ast_print(node->left,  depth + 1);
            ast_print(node->right, depth + 1);
            break;

        case NODE_VAR:
            printf("VAR  %s  (type: %s)\n",
                   node->sval, datatype_to_str(node->dtype));
            break;

        case NODE_INT_CONST:
            printf("INT_CONST  %d\n", node->ival);
            break;

        case NODE_BOOL_CONST:
            printf("BOOL_CONST  %s\n", node->ival ? "true" : "false");
            break;

        default:
            printf("UNKNOWN_NODE\n");
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Recursive free                                                       */
/* ------------------------------------------------------------------ */

void ast_free(ASTNode *node)
{
    if (!node) return;
    if (node->sval)  free(node->sval);
    ast_free(node->left);
    ast_free(node->right);
    ast_free(node->body);
    ast_free(node->els);
    for (int i = 0; i < node->stmt_count; i++)
        ast_free(node->stmts[i]);
    free(node->stmts);
    free(node);
}
