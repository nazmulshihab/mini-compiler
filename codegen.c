/*
 * codegen.c  –  TAC generation, optimization, and target code.
 *
 * Optimization passes (each repeated 3 times for convergence):
 *   Pass 1  constant folding  + algebraic simplification
 *   Pass 2  copy / constant propagation  (temporaries only)
 *   Pass 3  dead-code elimination
 *
 * Target code = stack-machine pseudo-assembly:
 *   PUSH <imm>  LOAD <var>  POP <var>
 *   ADD  SUB  MUL  DIV
 *   LT   GT   EQ   NEQ
 *   JMP <label>  JZ <label>
 *   PRINT   HALT   NOP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "codegen.h"

/* ================================================================== */
/* Section 1: TAC infrastructure                                       */
/* ================================================================== */

TACList *tac_create(void)
{
    TACList *tl = calloc(1, sizeof(TACList));
    if (!tl) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return tl;
}

/* Helper: strdup with NULL guard */
static char *xstrdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

char *tac_new_temp(TACList *tl)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", ++tl->temp_count);
    return strdup(buf);
}

char *tac_new_label(TACList *tl)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", ++tl->label_count);
    return strdup(buf);
}

static void tac_append(TACList *tl, TACInstr *i)
{
    if (!tl->head) tl->head = tl->tail = i;
    else           { tl->tail->next = i; tl->tail = i; }
}

TACInstr *tac_emit(TACList *tl, TACType type,
                   const char *result, const char *arg1,
                   const char *op,    const char *arg2)
{
    TACInstr *i = calloc(1, sizeof(TACInstr));
    i->type   = type;
    i->result = xstrdup(result);
    i->arg1   = xstrdup(arg1);
    i->op     = xstrdup(op);
    i->arg2   = xstrdup(arg2);
    tac_append(tl, i);
    return i;
}

TACInstr *tac_emit_label(TACList *tl, const char *label)
{
    TACInstr *i = calloc(1, sizeof(TACInstr));
    i->type  = TAC_LABEL;
    i->label = strdup(label);
    tac_append(tl, i);
    return i;
}

TACInstr *tac_emit_goto(TACList *tl, const char *label)
{
    TACInstr *i = calloc(1, sizeof(TACInstr));
    i->type  = TAC_GOTO;
    i->label = strdup(label);
    tac_append(tl, i);
    return i;
}

TACInstr *tac_emit_if_false(TACList *tl, const char *cond,
                             const char *label)
{
    TACInstr *i = calloc(1, sizeof(TACInstr));
    i->type  = TAC_IF_FALSE;
    i->arg1  = strdup(cond);
    i->label = strdup(label);
    tac_append(tl, i);
    return i;
}

TACInstr *tac_emit_print(TACList *tl, const char *arg)
{
    TACInstr *i = calloc(1, sizeof(TACInstr));
    i->type = TAC_PRINT;
    i->arg1 = strdup(arg);
    tac_append(tl, i);
    return i;
}

void tac_destroy(TACList *tl)
{
    TACInstr *cur = tl->head;
    while (cur) {
        TACInstr *nxt = cur->next;
        free(cur->result); free(cur->arg1);
        free(cur->op);     free(cur->arg2);
        free(cur->label);
        free(cur);
        cur = nxt;
    }
    free(tl);
}

/* ================================================================== */
/* Section 2: Code generation from the AST                            */
/* ================================================================== */

/*
 * codegen_expr – recursively lower an expression sub-tree into TAC.
 * Returns the name of the temp that holds the result (caller must free).
 */
char *codegen_expr(TACList *tl, ASTNode *node)
{
    if (!node) return strdup("?");

    switch (node->type) {

        case NODE_INT_CONST: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", node->ival);
            char *t = tac_new_temp(tl);
            tac_emit(tl, TAC_COPY, t, buf, NULL, NULL);
            return t;
        }

        case NODE_BOOL_CONST: {
            char *t = tac_new_temp(tl);
            tac_emit(tl, TAC_COPY, t, node->ival ? "1" : "0", NULL, NULL);
            return t;
        }

        case NODE_VAR: {
            char *t = tac_new_temp(tl);
            tac_emit(tl, TAC_COPY, t, node->sval, NULL, NULL);
            return t;
        }

        case NODE_BINOP: {
            char *lv = codegen_expr(tl, node->left);
            char *rv = codegen_expr(tl, node->right);
            char *t  = tac_new_temp(tl);
            tac_emit(tl, TAC_BINOP, t, lv, node->sval, rv);
            free(lv);
            free(rv);
            return t;
        }

        default:
            return strdup("?");
    }
}

/*
 * codegen_stmt – recursively lower a statement node into TAC.
 */
void codegen_stmt(TACList *tl, ASTNode *node)
{
    if (!node) return;

    switch (node->type) {

        case NODE_DECL:
            /* Pure declaration: no TAC needed; variables are modeled
               as named locations (the semantic phase already validated them). */
            break;

        case NODE_ASSIGN: {
            char *ev = codegen_expr(tl, node->right);
            tac_emit(tl, TAC_COPY, node->sval, ev, NULL, NULL);
            free(ev);
            break;
        }

        case NODE_IF: {
            char *lv_else = tac_new_label(tl);
            char *lv_end  = tac_new_label(tl);

            char *cond = codegen_expr(tl, node->left);
            tac_emit_if_false(tl, cond, lv_else);
            free(cond);

            /* Then-branch */
            codegen_stmt(tl, node->body);

            if (node->els) {
                tac_emit_goto(tl, lv_end);
            }
            tac_emit_label(tl, lv_else);

            /* Else-branch (optional) */
            if (node->els) {
                codegen_stmt(tl, node->els);
                tac_emit_label(tl, lv_end);
            }

            free(lv_else);
            free(lv_end);
            break;
        }

        case NODE_WHILE: {
            char *lv_start = tac_new_label(tl);
            char *lv_end   = tac_new_label(tl);

            tac_emit_label(tl, lv_start);

            char *cond = codegen_expr(tl, node->left);
            tac_emit_if_false(tl, cond, lv_end);
            free(cond);

            codegen_stmt(tl, node->body);
            tac_emit_goto(tl, lv_start);
            tac_emit_label(tl, lv_end);

            free(lv_start);
            free(lv_end);
            break;
        }

        case NODE_PRINT: {
            char *ev = codegen_expr(tl, node->left);
            tac_emit_print(tl, ev);
            free(ev);
            break;
        }

        case NODE_PROGRAM:
        case NODE_BLOCK:
            for (int i = 0; i < node->stmt_count; i++)
                codegen_stmt(tl, node->stmts[i]);
            break;

        default:
            break;
    }
}

TACList *codegen(ASTNode *root)
{
    TACList *tl = tac_create();
    codegen_stmt(tl, root);
    return tl;
}

/* ================================================================== */
/* Section 3: TAC text output                                         */
/* ================================================================== */

void tac_write(TACList *tl, FILE *out)
{
    fprintf(out, "; ============================================\n");
    fprintf(out, ";  Three-Address Code  –  MiniLang Compiler\n");
    fprintf(out, "; ============================================\n\n");

    for (TACInstr *i = tl->head; i; i = i->next) {
        if (i->is_dead) continue;
        switch (i->type) {
            case TAC_BINOP:
                fprintf(out, "    %s = %s %s %s\n",
                        i->result, i->arg1, i->op, i->arg2);
                break;
            case TAC_COPY:
                fprintf(out, "    %s = %s\n", i->result, i->arg1);
                break;
            case TAC_LABEL:
                fprintf(out, "%s:\n", i->label);
                break;
            case TAC_GOTO:
                fprintf(out, "    goto %s\n", i->label);
                break;
            case TAC_IF_FALSE:
                fprintf(out, "    ifFalse %s goto %s\n",
                        i->arg1, i->label);
                break;
            case TAC_PRINT:
                fprintf(out, "    print %s\n", i->arg1);
                break;
            case TAC_NOP:
                break;
        }
    }
    fprintf(out, "\n");
}

/* ================================================================== */
/* Section 4: Optimization                                            */
/* ================================================================== */

/* Returns 1 if 's' represents an integer constant */
static int is_int_const(const char *s)
{
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    while (*p) { if (!isdigit((unsigned char)*p)) return 0; p++; }
    return 1;
}

/* ------------------------------------------------------------------
   Pass 1: Constant folding + algebraic simplification
   ------------------------------------------------------------------ */

static int do_fold(int a, const char *op, int b, int *res)
{
    if (strcmp(op, "+")  == 0) { *res = a + b;        return 1; }
    if (strcmp(op, "-")  == 0) { *res = a - b;        return 1; }
    if (strcmp(op, "*")  == 0) { *res = a * b;        return 1; }
    if (strcmp(op, "/")  == 0) { if (!b) return 0; *res = a/b;  return 1; }
    if (strcmp(op, "<")  == 0) { *res = (a < b);      return 1; }
    if (strcmp(op, ">")  == 0) { *res = (a > b);      return 1; }
    if (strcmp(op, "==") == 0) { *res = (a == b);     return 1; }
    if (strcmp(op, "!=") == 0) { *res = (a != b);     return 1; }
    return 0;
}

/* Returns a freshly-allocated string if simplification applies, else NULL */
static char *algebraic_simplify(const char *op,
                                 const char *a, const char *b)
{
    /* x + 0  /  x - 0  → x */
    if ((strcmp(op,"+") == 0 || strcmp(op,"-") == 0) &&
         is_int_const(b) && atoi(b) == 0)
        return strdup(a);

    /* 0 + x → x */
    if (strcmp(op,"+") == 0 && is_int_const(a) && atoi(a) == 0)
        return strdup(b);

    /* x * 1  /  x / 1  → x */
    if ((strcmp(op,"*") == 0 || strcmp(op,"/") == 0) &&
         is_int_const(b) && atoi(b) == 1)
        return strdup(a);

    /* 1 * x → x */
    if (strcmp(op,"*") == 0 && is_int_const(a) && atoi(a) == 1)
        return strdup(b);

    /* x * 0  /  0 * x → 0 */
    if (strcmp(op,"*") == 0 &&
        ((is_int_const(b) && atoi(b) == 0) ||
         (is_int_const(a) && atoi(a) == 0)))
        return strdup("0");

    /* x - x → 0  (only when both sides are the same simple name) */
    if (strcmp(op,"-") == 0 && a && b && strcmp(a,b) == 0)
        return strdup("0");

    return NULL;
}

static void pass_constant_fold(TACList *tl)
{
    for (TACInstr *i = tl->head; i; i = i->next) {
        if (i->type != TAC_BINOP || i->is_dead) continue;

        /* Try algebraic simplification first (handles non-const args) */
        char *simplified = algebraic_simplify(i->op, i->arg1, i->arg2);
        if (simplified) {
            i->type = TAC_COPY;
            free(i->arg1); i->arg1 = simplified;
            free(i->arg2); i->arg2 = NULL;
            free(i->op);   i->op   = NULL;
            continue;
        }

        /* Full constant fold when both operands are integer literals */
        if (is_int_const(i->arg1) && is_int_const(i->arg2)) {
            int a = atoi(i->arg1), b = atoi(i->arg2), result;
            if (do_fold(a, i->op, b, &result)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", result);
                i->type = TAC_COPY;
                free(i->arg1); i->arg1 = strdup(buf);
                free(i->arg2); i->arg2 = NULL;
                free(i->op);   i->op   = NULL;
            }
        }
    }
}

/* ------------------------------------------------------------------
   Pass 2: Copy / constant propagation  (temporaries only)

   For each  ti = <value>  we forward-substitute <value> for ti in
   all subsequent instructions up to the next control-flow boundary
   or redefinition of ti.
   ------------------------------------------------------------------ */

static void pass_copy_propagation(TACList *tl)
{
    for (TACInstr *def = tl->head; def; def = def->next) {
        if (def->is_dead)  continue;
        if (def->type != TAC_COPY) continue;
        /* Only propagate temporaries (tN) */
        if (!def->result || def->result[0] != 't') continue;

        const char *from = def->result;
        const char *val  = def->arg1;          /* constant or another name */

        for (TACInstr *use = def->next; use; use = use->next) {
            /* Stop at control-flow boundaries – safe, conservative */
            if (use->type == TAC_LABEL || use->type == TAC_GOTO) break;

            /* Stop if 'from' is re-defined */
            if (use->result && strcmp(use->result, from) == 0) break;

            /* Substitute in arg1 */
            if (use->arg1 && strcmp(use->arg1, from) == 0) {
                free(use->arg1);
                use->arg1 = strdup(val);
            }
            /* Substitute in arg2 */
            if (use->arg2 && strcmp(use->arg2, from) == 0) {
                free(use->arg2);
                use->arg2 = strdup(val);
            }
            /* Substitute in condition (IF_FALSE / PRINT arg1 already handled) */
        }
    }
}

/* ------------------------------------------------------------------
   Pass 3: Dead-code elimination

   A temporary definition is dead when the temporary is never used
   anywhere in the instruction stream.
   ------------------------------------------------------------------ */

static int temp_is_used(TACList *tl, const char *name, TACInstr *skip)
{
    for (TACInstr *i = tl->head; i; i = i->next) {
        if (i == skip || i->is_dead) continue;
        if ((i->arg1  && strcmp(i->arg1,  name) == 0) ||
            (i->arg2  && strcmp(i->arg2,  name) == 0))
            return 1;
    }
    return 0;
}

static void pass_dead_code(TACList *tl)
{
    for (TACInstr *i = tl->head; i; i = i->next) {
        if (i->is_dead) continue;
        if (i->type != TAC_COPY && i->type != TAC_BINOP) continue;
        /* Only eliminate definitions of temporaries */
        if (!i->result || i->result[0] != 't') continue;

        if (!temp_is_used(tl, i->result, i))
            i->is_dead = 1;
    }
}

/* ------------------------------------------------------------------
   Public optimizer entry point
   ------------------------------------------------------------------ */

void optimize(TACList *tl)
{
    /* Three iterations to reach a fixpoint for chained optimizations */
    for (int iter = 0; iter < 3; iter++) {
        pass_constant_fold(tl);
        pass_copy_propagation(tl);
        pass_dead_code(tl);
    }
}

/* ================================================================== */
/* Section 5: Target code  (stack-machine pseudo-assembly)            */
/* ================================================================== */
/*
 * Virtual machine model
 * ---------------------
 *   PUSH <imm>  : push integer immediate onto operand stack
 *   LOAD <var>  : push value of named variable/temp onto stack
 *   POP  <var>  : pop top of stack → named variable/temp
 *   ADD / SUB / MUL / DIV : pop two, push result
 *   LT  / GT  / EQ  / NEQ : pop two, push 0/1 comparison result
 *   JMP  <label> : unconditional jump
 *   JZ   <label> : pop top; jump if zero (false)
 *   PRINT        : pop top; output it
 *   HALT         : stop execution
 *   NOP          : no operation
 */

void target_codegen(TACList *tl, FILE *out)
{
    fprintf(out, "; ============================================\n");
    fprintf(out, ";  Target Code (Stack Machine Pseudo-Assembly)\n");
    fprintf(out, ";  MiniLang Compiler\n");
    fprintf(out, "; ============================================\n");
    fprintf(out, ";\n");
    fprintf(out, ";  PUSH <imm>  – push integer constant\n");
    fprintf(out, ";  LOAD <var>  – load variable onto stack\n");
    fprintf(out, ";  POP  <var>  – pop stack into variable\n");
    fprintf(out, ";  ADD SUB MUL DIV  – arithmetic\n");
    fprintf(out, ";  LT  GT  EQ  NEQ – comparison (result: 0 or 1)\n");
    fprintf(out, ";  JMP <label> – unconditional branch\n");
    fprintf(out, ";  JZ  <label> – branch if top-of-stack == 0\n");
    fprintf(out, ";  PRINT        – pop and output\n");
    fprintf(out, ";  HALT         – terminate\n");
    fprintf(out, ";\n\n");

    for (TACInstr *i = tl->head; i; i = i->next) {
        if (i->is_dead) continue;

        switch (i->type) {

            case TAC_COPY:
                /* result = arg1 */
                if (is_int_const(i->arg1))
                    fprintf(out, "    PUSH %s\n", i->arg1);
                else
                    fprintf(out, "    LOAD %s\n", i->arg1);
                fprintf(out, "    POP  %s\n", i->result);
                break;

            case TAC_BINOP: {
                /* Push both operands, apply operation, store result */
                if (is_int_const(i->arg1))
                    fprintf(out, "    PUSH %s\n", i->arg1);
                else
                    fprintf(out, "    LOAD %s\n", i->arg1);

                if (is_int_const(i->arg2))
                    fprintf(out, "    PUSH %s\n", i->arg2);
                else
                    fprintf(out, "    LOAD %s\n", i->arg2);

                if      (strcmp(i->op, "+")  == 0) fprintf(out, "    ADD\n");
                else if (strcmp(i->op, "-")  == 0) fprintf(out, "    SUB\n");
                else if (strcmp(i->op, "*")  == 0) fprintf(out, "    MUL\n");
                else if (strcmp(i->op, "/")  == 0) fprintf(out, "    DIV\n");
                else if (strcmp(i->op, "<")  == 0) fprintf(out, "    LT\n");
                else if (strcmp(i->op, ">")  == 0) fprintf(out, "    GT\n");
                else if (strcmp(i->op, "==") == 0) fprintf(out, "    EQ\n");
                else if (strcmp(i->op, "!=") == 0) fprintf(out, "    NEQ\n");

                fprintf(out, "    POP  %s\n", i->result);
                break;
            }

            case TAC_LABEL:
                fprintf(out, "%s:\n", i->label);
                break;

            case TAC_GOTO:
                fprintf(out, "    JMP %s\n", i->label);
                break;

            case TAC_IF_FALSE:
                if (is_int_const(i->arg1))
                    fprintf(out, "    PUSH %s\n", i->arg1);
                else
                    fprintf(out, "    LOAD %s\n", i->arg1);
                fprintf(out, "    JZ  %s\n", i->label);
                break;

            case TAC_PRINT:
                if (is_int_const(i->arg1))
                    fprintf(out, "    PUSH %s\n", i->arg1);
                else
                    fprintf(out, "    LOAD %s\n", i->arg1);
                fprintf(out, "    PRINT\n");
                break;

            case TAC_NOP:
                fprintf(out, "    NOP\n");
                break;
        }
    }

    fprintf(out, "    HALT\n");
}
