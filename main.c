// /*
//  * main.c  –  MiniLang compiler driver.
//  *
//  * Compilation pipeline
//  * --------------------
//  *  Phase 1  Lexical analysis + Parsing   (flex / bison)
//  *  Phase 2  Semantic analysis            (semantic.c)
//  *  Phase 3  TAC generation               (codegen.c)
//  *  Phase 4  Optimisation                 (codegen.c – optimize())
//  *  Phase 5  Target code generation       (codegen.c – target_codegen())
//  *
//  * Usage
//  * -----
//  *   ./minicompiler <source.ml> [--dump-ast] [--no-opt] [--dump-sym]
//  *
//  *   --dump-ast   Print the AST to stdout after parsing.
//  *   --dump-sym   (reserved; symbol table is printed on request)
//  *   --no-opt     Skip optimisation pass; emit raw TAC.
//  *
//  * Output files
//  * ------------
//  *   output.tac   Three-address code (after optional optimisation)
//  *   output.asm   Stack-machine pseudo-assembly
//  */

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include "ast.h"
// #include "semantic.h"
// #include "codegen.h"

// /* Symbols provided by the bison/flex generated sources */
// extern FILE    *yyin;
// extern int      yyparse(void);
// extern ASTNode *parse_tree;

// /* ------------------------------------------------------------------ */
// /* Helpers                                                             */
// /* ------------------------------------------------------------------ */

// static void banner(void)
// {
//     printf("╔══════════════════════════════════════╗\n");
//     printf("║     MiniLang Compiler  v1.0          ║\n");
//     printf("║  Lex → Parse → Sem → TAC → Asm       ║\n");
//     printf("╚══════════════════════════════════════╝\n");
// }

// static void phase(int n, const char *desc)
// {
//     printf("\n[Phase %d/5]  %s\n", n, desc);
//     printf("            ");
//     for (int i = 0; (size_t)i < strlen(desc); i++) putchar('-');
//     putchar('\n');
// }

// static void ok_msg(const char *detail)
// {
//     printf("  ✓  %s\n", detail);
// }

// /* ------------------------------------------------------------------ */
// /* main                                                                */
// /* ------------------------------------------------------------------ */

// int main(int argc, char *argv[])
// {
//     if (argc < 2) {
//         fprintf(stderr,
//             "Usage: %s <source.ml> [--dump-ast] [--no-opt] [--dump-sym]\n",
//             argv[0]);
//         return 1;
//     }

//     /* ---- Parse flags ---- */
//     int dump_ast = 0;
//     int no_opt   = 0;
//     int dump_sym = 0;

//     for (int i = 2; i < argc; i++) {
//         if (strcmp(argv[i], "--dump-ast") == 0) dump_ast = 1;
//         if (strcmp(argv[i], "--no-opt")   == 0) no_opt   = 1;
//         if (strcmp(argv[i], "--dump-sym") == 0) dump_sym = 1;
//     }

//     banner();
//     printf("\n  Source file : %s\n", argv[1]);

//     /* ================================================================
//        Phase 1: Lexical analysis + Parsing
//        ================================================================ */
//     phase(1, "Lexical Analysis & Parsing");

//     FILE *src = fopen(argv[1], "r");
//     if (!src) {
//         fprintf(stderr, "  Error: cannot open '%s'\n", argv[1]);
//         return 1;
//     }
//     yyin = src;

//     int parse_result = yyparse();
//     fclose(src);

//     if (parse_result != 0 || !parse_tree) {
//         fprintf(stderr,
//             "\n  Compilation aborted: syntax error(s) found.\n");
//         return 1;
//     }
//     ok_msg("Parsing complete – AST built.");

//     if (dump_ast) {
//         printf("\n  === Abstract Syntax Tree ===\n");
//         ast_print(parse_tree, 2);
//         printf("  ============================\n");
//     }

//     /* ================================================================
//        Phase 2: Semantic Analysis
//        ================================================================ */
//     phase(2, "Semantic Analysis");

//     SemanticContext *sem = semantic_create();

//     if (dump_sym) {
//         /* Not meaningful before analysis runs; kept as a hook */
//         printf("  (Symbol table dump requested – shown after analysis)\n");
//     }

//     int sem_errors = semantic_analyze(sem, parse_tree);
//     semantic_destroy(sem);

//     if (sem_errors > 0) {
//         fprintf(stderr,
//             "\n  Compilation aborted: %d semantic error(s) found.\n",
//             sem_errors);
//         ast_free(parse_tree);
//         return 1;
//     }
//     ok_msg("No semantic errors. AST type-annotated.");

//     /* ================================================================
//        Phase 3: TAC Generation
//        ================================================================ */
//     phase(3, "Intermediate Code Generation  (TAC)");

//     TACList *tac = codegen(parse_tree);
//     ok_msg("Three-Address Code generated.");

//     /* ================================================================
//        Phase 4: Optimisation
//        ================================================================ */
//     phase(4, "Optimisation");

//     if (no_opt) {
//         printf("  (skipped via --no-opt)\n");
//     } else {
//         optimize(tac);
//         ok_msg("Passes: constant folding, copy propagation, dead-code elimination.");
//     }

//     /* Write TAC output */
//     FILE *tac_fp = fopen("output.tac", "w");
//     if (!tac_fp) {
//         fprintf(stderr, "  Warning: cannot write output.tac\n");
//     } else {
//         tac_write(tac, tac_fp);
//         fclose(tac_fp);
//         ok_msg("TAC written  → output.tac");
//     }

//     /* ================================================================
//        Phase 5: Target Code Generation
//        ================================================================ */
//     phase(5, "Target Code Generation  (Stack Machine)");

//     FILE *asm_fp = fopen("output.asm", "w");
//     if (!asm_fp) {
//         fprintf(stderr, "  Warning: cannot write output.asm\n");
//     } else {
//         target_codegen(tac, asm_fp);
//         fclose(asm_fp);
//         ok_msg("Assembly written → output.asm");
//     }

//     /* ================================================================
//        Cleanup
//        ================================================================ */
//     tac_destroy(tac);
//     ast_free(parse_tree);

//     printf("\n══════════════════════════════════════════\n");
//     printf("  Compilation SUCCESSFUL\n");
//     printf("  output.tac   –  optimised three-address code\n");
//     printf("  output.asm   –  stack-machine pseudo-assembly\n");
//     printf("══════════════════════════════════════════\n\n");

//     return 0;
// }

// ---------------------------------

/*
 * main.c  –  MiniLang compiler driver.
 *
 * Compilation pipeline
 * --------------------
 *  Phase 1  Lexical analysis + Parsing   (flex / bison)
 *  Phase 2  Semantic analysis            (semantic.c)
 *  Phase 3  TAC generation               (codegen.c)
 *  Phase 4  Optimisation                 (codegen.c – optimize())
 *  Phase 5  Target code generation       (codegen.c – target_codegen())
 *
 * Usage
 * -----
 *   ./minicompiler <source.ml> [--dump-ast] [--no-opt] [--dump-sym]
 *
 * Output files
 * ------------
 *   output.tac   Three-address code (after optional optimisation)
 *   output.asm   Stack-machine pseudo-assembly
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

/* Symbols provided by the bison/flex generated sources */
extern FILE    *yyin;
extern int      yyparse(void);
extern ASTNode *parse_tree;
extern int      syntax_error_count;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void banner(void)
{
    printf("╔══════════════════════════════════════╗\n");
    printf("║     MiniLang Compiler  v1.0          ║\n");
    printf("║  Lex → Parse → Sem → TAC → Asm       ║\n");
    printf("╚══════════════════════════════════════╝\n");
}

static void phase(int n, const char *desc)
{
    printf("\n[Phase %d/5]  %s\n", n, desc);
    printf("            ");
    for (size_t i = 0; i < strlen(desc); i++) putchar('-');
    putchar('\n');
}

static void ok_msg(const char *detail)
{
    printf("  ✓  %s\n", detail);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <source.ml> [--dump-ast] [--no-opt] [--dump-sym]\n",
            argv[0]);
        return 1;
    }

    /* ---- Parse flags ---- */
    int dump_ast = 0;
    int no_opt   = 0;
    int dump_sym = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dump-ast") == 0) dump_ast = 1;
        else if (strcmp(argv[i], "--no-opt") == 0) no_opt = 1;
        else if (strcmp(argv[i], "--dump-sym") == 0) dump_sym = 1;
    }

    banner();
    printf("\n  Source file : %s\n", argv[1]);

    /* ================================================================
       Phase 1: Lexical analysis + Parsing
       ================================================================ */
    phase(1, "Lexical Analysis & Parsing");

    FILE *src = fopen(argv[1], "r");
    if (!src) {
        fprintf(stderr, "  Error: cannot open '%s'\n", argv[1]);
        return 1;
    }

    yyin = src;
    parse_tree = NULL;
    syntax_error_count = 0;

    int parse_result = yyparse();
    fclose(src);

    if (parse_result != 0 || syntax_error_count > 0 || !parse_tree) {
        fprintf(stderr,
            "\n  Compilation aborted: %d syntax error(s) found.\n",
            syntax_error_count);
        if (parse_tree) {
            ast_free(parse_tree);
            parse_tree = NULL;
        }
        return 1;
    }

    ok_msg("Parsing complete – AST built.");

    if (dump_ast) {
        printf("\n  === Abstract Syntax Tree ===\n");
        ast_print(parse_tree, 2);
        printf("  ============================\n");
    }

    /* ================================================================
       Phase 2: Semantic Analysis
       ================================================================ */
    phase(2, "Semantic Analysis");

    SemanticContext *sem = semantic_create();

    if (dump_sym) {
        printf("  (Symbol table dump requested – shown after analysis)\n");
    }

    int sem_errors = semantic_analyze(sem, parse_tree);
    semantic_destroy(sem);

    if (sem_errors > 0) {
        fprintf(stderr,
            "\n  Compilation aborted: %d semantic error(s) found.\n",
            sem_errors);
        ast_free(parse_tree);
        return 1;
    }

    ok_msg("No semantic errors. AST type-annotated.");

    /* ================================================================
       Phase 3: TAC Generation
       ================================================================ */
    phase(3, "Intermediate Code Generation  (TAC)");

    TACList *tac = codegen(parse_tree);
    ok_msg("Three-Address Code generated.");

    /* ================================================================
       Phase 4: Optimisation
       ================================================================ */
    phase(4, "Optimisation");

    if (no_opt) {
        printf("  (skipped via --no-opt)\n");
    } else {
        optimize(tac);
        ok_msg("Passes: constant folding, copy propagation, dead-code elimination.");
    }

    FILE *tac_fp = fopen("output.tac", "w");
    if (!tac_fp) {
        fprintf(stderr, "  Warning: cannot write output.tac\n");
    } else {
        tac_write(tac, tac_fp);
        fclose(tac_fp);
        ok_msg("TAC written  → output.tac");
    }

    /* ================================================================
       Phase 5: Target Code Generation
       ================================================================ */
    phase(5, "Target Code Generation  (Stack Machine)");

    FILE *asm_fp = fopen("output.asm", "w");
    if (!asm_fp) {
        fprintf(stderr, "  Warning: cannot write output.asm\n");
    } else {
        target_codegen(tac, asm_fp);
        fclose(asm_fp);
        ok_msg("Assembly written → output.asm");
    }

    /* ================================================================
       Cleanup
       ================================================================ */
    tac_destroy(tac);
    ast_free(parse_tree);
    parse_tree = NULL;

    printf("\n══════════════════════════════════════════\n");
    printf("  Compilation SUCCESSFUL\n");
    printf("  output.tac   –  optimised three-address code\n");
    printf("  output.asm   –  stack-machine pseudo-assembly\n");
    printf("══════════════════════════════════════════\n\n");

    return 0;
}
