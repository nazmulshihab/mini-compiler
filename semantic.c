/*
 * semantic.c  –  Semantic analysis implementation.
 *
 * Phase responsibilities
 * ----------------------
 *  1. Maintain a scoped symbol table while traversing the AST.
 *  2. Annotate every node with its resolved DataType (dtype field).
 *  3. Report all semantic errors with source line numbers; continue
 *     after each error so multiple problems are surfaced in one run.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

SemanticContext *semantic_create(void)
{
    SemanticContext *ctx = calloc(1, sizeof(SemanticContext));
    ctx->symtable = symtable_create();
    return ctx;
}

void semantic_destroy(SemanticContext *ctx)
{
    symtable_destroy(ctx->symtable);
    free(ctx);
}

/* ------------------------------------------------------------------ */
/* Error helper                                                        */
/* ------------------------------------------------------------------ */

static void sem_error(SemanticContext *ctx, int line, const char *msg)
{
    fprintf(stderr, "Semantic Error [line %d]: %s\n", line, msg);
    ctx->error_count++;
}

/* ------------------------------------------------------------------ */
/* Expression type-checking                                            */
/* ------------------------------------------------------------------ */

DataType semantic_check_expr(SemanticContext *ctx, ASTNode *node)
{
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {

        /* ---- literals ---- */
        case NODE_INT_CONST:
            node->dtype = TYPE_INT;
            return TYPE_INT;

        case NODE_BOOL_CONST:
            node->dtype = TYPE_BOOL;
            return TYPE_BOOL;

        /* ---- variable reference ---- */
        case NODE_VAR: {
            SymEntry *e = symtable_lookup(ctx->symtable, node->sval);
            if (!e) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Undeclared variable '%s'", node->sval);
                sem_error(ctx, node->line, msg);
                node->dtype = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            node->dtype = e->type;
            return e->type;
        }

        /* ---- binary operators ---- */
        case NODE_BINOP: {
            DataType lt = semantic_check_expr(ctx, node->left);
            DataType rt = semantic_check_expr(ctx, node->right);
            const char *op = node->sval;

            /* Propagate unknown upward without a second error */
            if (lt == TYPE_UNKNOWN || rt == TYPE_UNKNOWN) {
                node->dtype = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }

            /* Arithmetic: +  -  *  /  require int × int → int */
            if (strcmp(op,"+") == 0 || strcmp(op,"-") == 0 ||
                strcmp(op,"*") == 0 || strcmp(op,"/") == 0)
            {
                if (lt != TYPE_INT || rt != TYPE_INT) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Operator '%s' requires int operands "
                             "(got %s and %s)",
                             op, datatype_to_str(lt), datatype_to_str(rt));
                    sem_error(ctx, node->line, msg);
                    node->dtype = TYPE_UNKNOWN;
                    return TYPE_UNKNOWN;
                }
                node->dtype = TYPE_INT;
                return TYPE_INT;
            }

            /* Relational: <  >  ==  !=  require same type → bool */
            if (strcmp(op,"<")  == 0 || strcmp(op,">")  == 0 ||
                strcmp(op,"==") == 0 || strcmp(op,"!=") == 0)
            {
                if (lt != rt) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Operator '%s' requires same-type operands "
                             "(got %s and %s)",
                             op, datatype_to_str(lt), datatype_to_str(rt));
                    sem_error(ctx, node->line, msg);
                    node->dtype = TYPE_UNKNOWN;
                    return TYPE_UNKNOWN;
                }
                node->dtype = TYPE_BOOL;
                return TYPE_BOOL;
            }

            /* Should not be reached for a valid parse */
            node->dtype = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }

        default:
            node->dtype = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
    }
}

/* ------------------------------------------------------------------ */
/* Statement checking (forward declaration)                            */
/* ------------------------------------------------------------------ */

static void check_stmt(SemanticContext *ctx, ASTNode *node);

/* ------------------------------------------------------------------ */
/* Block: open a scope, check every statement, close the scope.        */
/* ------------------------------------------------------------------ */

static void check_block(SemanticContext *ctx, ASTNode *node)
{
    symtable_enter_scope(ctx->symtable);
    for (int i = 0; i < node->stmt_count; i++)
        check_stmt(ctx, node->stmts[i]);
    symtable_exit_scope(ctx->symtable);
}

/* ------------------------------------------------------------------ */
/* Statement checking                                                  */
/* ------------------------------------------------------------------ */

static void check_stmt(SemanticContext *ctx, ASTNode *node)
{
    if (!node) return;

    switch (node->type) {

        /* ---- declaration ---- */
        case NODE_DECL: {
            if (symtable_declare(ctx->symtable,
                                 node->sval, node->decl_type) < 0) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Duplicate declaration of '%s' in the same scope",
                         node->sval);
                sem_error(ctx, node->line, msg);
            }
            break;
        }

        /* ---- assignment ---- */
        case NODE_ASSIGN: {
            SymEntry *e = symtable_lookup(ctx->symtable, node->sval);
            if (!e) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Assignment to undeclared variable '%s'",
                         node->sval);
                sem_error(ctx, node->line, msg);
            }
            DataType et = semantic_check_expr(ctx, node->right);
            if (e && et != TYPE_UNKNOWN && e->type != et) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Type mismatch: cannot assign %s to '%s' (type %s)",
                         datatype_to_str(et),
                         node->sval,
                         datatype_to_str(e->type));
                sem_error(ctx, node->line, msg);
            }
            node->dtype = e ? e->type : TYPE_UNKNOWN;
            break;
        }

        /* ---- if / else ---- */
        case NODE_IF: {
            DataType ct = semantic_check_expr(ctx, node->left);
            if (ct != TYPE_BOOL && ct != TYPE_UNKNOWN) {
                sem_error(ctx, node->line,
                          "Condition in 'if' must be of type bool");
            }
            /* then / else branches are always BLOCK nodes (grammar enforces) */
            check_stmt(ctx, node->body);
            if (node->els)
                check_stmt(ctx, node->els);
            break;
        }

        /* ---- while ---- */
        case NODE_WHILE: {
            DataType ct = semantic_check_expr(ctx, node->left);
            if (ct != TYPE_BOOL && ct != TYPE_UNKNOWN) {
                sem_error(ctx, node->line,
                          "Condition in 'while' must be of type bool");
            }
            check_stmt(ctx, node->body);
            break;
        }

        /* ---- print ---- */
        case NODE_PRINT:
            semantic_check_expr(ctx, node->left);
            break;

        /* ---- nested block ---- */
        case NODE_BLOCK:
            check_block(ctx, node);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int semantic_analyze(SemanticContext *ctx, ASTNode *root)
{
    if (!root) return 0;

    /* The program-level scope */
    symtable_enter_scope(ctx->symtable);
    for (int i = 0; i < root->stmt_count; i++)
        check_stmt(ctx, root->stmts[i]);
    symtable_exit_scope(ctx->symtable);

    return ctx->error_count;
}
