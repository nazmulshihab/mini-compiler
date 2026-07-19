/*
 * semantic.h  –  Semantic analysis (type checking + scoped symbol table).
 *
 * Catches:
 *   • undeclared variable references
 *   • duplicate declarations in the same scope
 *   • type mismatches in assignments and binary operators
 *   • non-bool conditions in if / while
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"

typedef struct {
    int       error_count;
    SymTable *symtable;
} SemanticContext;

SemanticContext *semantic_create  (void);
void             semantic_destroy (SemanticContext *ctx);

/*
 * Walks the whole AST; annotates each node's dtype field.
 * Returns the number of semantic errors found (0 = success).
 */
int      semantic_analyze    (SemanticContext *ctx, ASTNode *root);

/*
 * Recursively type-checks a sub-expression and returns its DataType.
 * Called by semantic_analyze; also exposed for testing.
 */
DataType semantic_check_expr (SemanticContext *ctx, ASTNode *node);

#endif /* SEMANTIC_H */
