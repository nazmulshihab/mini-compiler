/*
 * symbol_table.c  –  Scoped symbol-table implementation.
 *
 * Each scope is an independent hash table.  Entering a block pushes
 * a new scope; leaving pops and frees it.  Lookup walks inward-to-
 * outward so inner declarations shadow outer ones correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

/* ------------------------------------------------------------------ */
/* djb2 hash                                                           */
/* ------------------------------------------------------------------ */
static unsigned int hash_str(const char *s)
{
    unsigned int h = 5381u;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h % HASH_SIZE;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

SymTable *symtable_create(void)
{
    SymTable *st = calloc(1, sizeof(SymTable));
    if (!st) { fprintf(stderr, "Out of memory\n"); exit(1); }
    st->current_level = -1;
    return st;
}

void symtable_enter_scope(SymTable *st)
{
    st->current_level++;
    if (st->current_level >= MAX_SCOPE_DEPTH) {
        fprintf(stderr, "Fatal: scope depth limit (%d) exceeded\n",
                MAX_SCOPE_DEPTH);
        exit(1);
    }
    st->scopes[st->current_level] = calloc(1, sizeof(Scope));
    if (!st->scopes[st->current_level]) {
        fprintf(stderr, "Out of memory\n"); exit(1);
    }
}

void symtable_exit_scope(SymTable *st)
{
    if (st->current_level < 0) {
        fprintf(stderr, "Internal error: symtable_exit_scope on empty stack\n");
        return;
    }
    Scope *s = st->scopes[st->current_level];
    for (int b = 0; b < HASH_SIZE; b++) {
        SymEntry *e = s->buckets[b];
        while (e) {
            SymEntry *nxt = e->next;
            free(e->name);
            free(e);
            e = nxt;
        }
    }
    free(s);
    st->scopes[st->current_level] = NULL;
    st->current_level--;
}

void symtable_destroy(SymTable *st)
{
    while (st->current_level >= 0)
        symtable_exit_scope(st);
    free(st);
}

/* ------------------------------------------------------------------ */
/* declare                                                             */
/* ------------------------------------------------------------------ */

int symtable_declare(SymTable *st, const char *name, DataType type)
{
    if (st->current_level < 0) return -1;

    /* Reject duplicate in the same (current) scope */
    if (symtable_lookup_current(st, name)) return -1;

    Scope    *s = st->scopes[st->current_level];
    unsigned  h = hash_str(name);

    SymEntry *e  = calloc(1, sizeof(SymEntry));
    e->name        = strdup(name);
    e->type        = type;
    e->scope_level = st->current_level;
    e->next        = s->buckets[h];
    s->buckets[h]  = e;
    return 0;
}

/* ------------------------------------------------------------------ */
/* lookup – inner-most scope first                                     */
/* ------------------------------------------------------------------ */

SymEntry *symtable_lookup(SymTable *st, const char *name)
{
    for (int lv = st->current_level; lv >= 0; lv--) {
        Scope    *s = st->scopes[lv];
        unsigned  h = hash_str(name);
        SymEntry *e = s->buckets[h];
        while (e) {
            if (strcmp(e->name, name) == 0) return e;
            e = e->next;
        }
    }
    return NULL;
}

SymEntry *symtable_lookup_current(SymTable *st, const char *name)
{
    if (st->current_level < 0) return NULL;
    Scope    *s = st->scopes[st->current_level];
    unsigned  h = hash_str(name);
    SymEntry *e = s->buckets[h];
    while (e) {
        if (strcmp(e->name, name) == 0) return e;
        e = e->next;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Debug dump                                                          */
/* ------------------------------------------------------------------ */

void symtable_print(SymTable *st)
{
    printf("=== Symbol Table  (depth %d) ===\n", st->current_level);
    for (int lv = 0; lv <= st->current_level; lv++) {
        printf("  Scope %d:\n", lv);
        Scope *s = st->scopes[lv];
        for (int b = 0; b < HASH_SIZE; b++) {
            SymEntry *e = s->buckets[b];
            while (e) {
                printf("    %-20s : %s\n",
                       e->name, datatype_to_str(e->type));
                e = e->next;
            }
        }
    }
    printf("================================\n");
}
