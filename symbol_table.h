/*
 * symbol_table.h  –  Scoped symbol table for MiniLang.
 *
 * Implemented as a stack of hash-table scopes.
 * enter_scope / exit_scope track block nesting;
 * declare / lookup give O(1) average access per scope level.
 */

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
#define HASH_SIZE       256
#define MAX_SCOPE_DEPTH  64

/* ------------------------------------------------------------------ */
/* A single symbol-table entry                                         */
/* ------------------------------------------------------------------ */
typedef struct SymEntry {
    char            *name;
    DataType         type;
    int              scope_level;
    struct SymEntry *next;        /* hash-chain for the same bucket   */
} SymEntry;

/* ------------------------------------------------------------------ */
/* One scope level = one hash table                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    SymEntry *buckets[HASH_SIZE];
} Scope;

/* ------------------------------------------------------------------ */
/* Full symbol table (stack of scopes)                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    Scope *scopes[MAX_SCOPE_DEPTH];
    int    current_level;         /* -1 = no scope open               */
} SymTable;

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */
SymTable *symtable_create        (void);
void      symtable_destroy       (SymTable *st);

void      symtable_enter_scope   (SymTable *st);
void      symtable_exit_scope    (SymTable *st);

/* Returns 0 on success, -1 on duplicate in current scope */
int       symtable_declare       (SymTable *st,
                                  const char *name, DataType type);

/* Searches all enclosing scopes (inner-to-outer) */
SymEntry *symtable_lookup        (SymTable *st, const char *name);

/* Searches current scope only */
SymEntry *symtable_lookup_current(SymTable *st, const char *name);

void      symtable_print         (SymTable *st);

#endif /* SYMBOL_TABLE_H */
