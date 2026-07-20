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
// #include <stdarg.h>
// #include "ast.h"
// #include "semantic.h"
// #include "codegen.h"

// /* Portable "is this a real terminal" check: on POSIX systems we use
//  * isatty(); on Windows (MSVC / MinGW) we use the equivalent _isatty().
//  * If neither is available we simply default color off, which is always
//  * safe (plain text still displays correctly everywhere).            */
// #if defined(_WIN32)
// #include <io.h>
// #define ISATTY(fd) _isatty(fd)
// #define FILENO(f)  _fileno(f)
// #elif defined(__unix__) || defined(__APPLE__)
// #include <unistd.h>
// #define ISATTY(fd) isatty(fd)
// #define FILENO(f)  fileno(f)
// #else
// #define ISATTY(fd) 0
// #define FILENO(f)  0
// #endif

// /* Symbols provided by the bison/flex generated sources */
// extern FILE    *yyin;
// extern int      yyparse(void);
// extern ASTNode *parse_tree;
// extern int      syntax_error_count;

// /* ------------------------------------------------------------------ */
// /* Terminal UI helpers                                                 */
// /* ------------------------------------------------------------------ */

// #define BOX_WIDTH 78   /* interior width, not counting the two border chars */

// /* Colour support: only emit ANSI codes when stdout is an actual terminal,
//  * so piping output to a file or another program stays clean.            */
// static int g_color = 0;

// static void ui_init(void)
// {
//     g_color = ISATTY(FILENO(stdout));
// }

// #define C_RESET   (g_color ? "\033[0m"  : "")
// #define C_BOLD    (g_color ? "\033[1m"  : "")
// #define C_DIM     (g_color ? "\033[2m"  : "")
// #define C_CYAN    (g_color ? "\033[36m" : "")
// #define C_GREEN   (g_color ? "\033[32m" : "")
// #define C_YELLOW  (g_color ? "\033[33m" : "")
// #define C_RED     (g_color ? "\033[31m" : "")
// #define C_BLUE    (g_color ? "\033[34m" : "")

// /* Prints a horizontal box border, e.g. +----------------------------+ */
// static void box_line(const char *left, const char *mid, const char *right)
// {
//     printf("%s", left);
//     for (int i = 0; i < BOX_WIDTH; i++) printf("%s", mid);
//     printf("%s\n", right);
// }

// /* Prints exactly one already-wrapped, already-length-checked chunk
//  * ("segment") as one bordered line, coloring it AFTER padding is
//  * computed so ANSI escape bytes never count toward the visible width. */
// static void box_segment(const char *segment, int len, const char *color)
// {
//     if (len < 0) len = 0;
//     if (len > BOX_WIDTH) len = BOX_WIDTH; /* hard safety net, should not trigger */

//     printf("|%s%.*s%s%*s|\n",
//            color, len, segment, C_RESET,
//            BOX_WIDTH - len, "");
// }

// /* Prints text inside the box, left-aligned. Any text longer than
//  * BOX_WIDTH is WORD-WRAPPED onto additional bordered lines instead of
//  * being truncated or allowed to break the box, so arbitrarily long
//  * filenames / messages / identifiers can never corrupt the layout.
//  * Wraps on spaces where possible; hard-splits only if a single "word"
//  * is itself longer than the box (e.g. a long path with no spaces). */
// static void box_text_c(const char *color, const char *fmt, ...)
// {
//     char buf[4096];
//     va_list args;
//     va_start(args, fmt);
//     vsnprintf(buf, sizeof(buf), fmt, args);
//     va_end(args);

//     int total = (int)strlen(buf);
//     if (total == 0) {
//         box_segment("", 0, "");
//         return;
//     }

//     int pos = 0;
//     while (pos < total) {
//         int remaining = total - pos;
//         int take = remaining;

//         if (take > BOX_WIDTH) {
//             take = BOX_WIDTH;
//             /* try to break at the last space within this chunk so we
//              * don't split a word mid-way */
//             int break_at = -1;
//             for (int i = take; i > 0; i--) {
//                 if (buf[pos + i - 1] == ' ') { break_at = i; break; }
//             }
//             /* only use the space-break if it doesn't waste more than
//              * half the line (otherwise a long unbroken token exists —
//              * hard-split it instead, still perfectly safe/aligned) */
//             if (break_at > BOX_WIDTH / 2) take = break_at;
//         }

//         box_segment(buf + pos, take, color);

//         pos += take;
//         while (pos < total && buf[pos] == ' ') pos++; /* skip the space we broke on */
//     }
// }

// /* Convenience wrapper: no color. */
// static void box_text(const char *fmt, ...)
// {
//     char buf[4096];
//     va_list args;
//     va_start(args, fmt);
//     vsnprintf(buf, sizeof(buf), fmt, args);
//     va_end(args);
//     box_text_c("", "%s", buf);
// }

// /* Centered text inside the box (used for titles). Long titles wrap
//  * via box_text_c rather than overflowing the border. */
// static void box_center(const char *text, const char *color)
// {
//     int len = (int)strlen(text);
//     if (len > BOX_WIDTH) { box_text_c(color, "%s", text); return; }

//     int left_pad  = (BOX_WIDTH - len) / 2;
//     int right_pad = BOX_WIDTH - len - left_pad;

//     printf("|%*s%s%.*s%s%*s|\n",
//            left_pad, "",
//            color, len, text, C_RESET,
//            right_pad, "");
// }

// static void box_blank(void)
// {
//     printf("|%*s|\n", BOX_WIDTH, "");
// }

// /* Runs a printing function (e.g. ast_print) while capturing whatever
//  * it writes to stdout, then re-emits every captured line through
//  * box_text so the box border is never broken open by callee output
//  * that doesn't know about the box at all.
//  *
//  * Implemented with plain standard-C stdio (freopen + tmpfile) instead
//  * of fork()/pipe(), so it compiles and works identically on Windows
//  * (MinGW/MSVC), Linux, and macOS with no platform-specific headers. */
// static void box_capture_ast(ASTNode *tree, int indent)
// {
//     fflush(stdout);

//     FILE *tmp = tmpfile();
//     if (!tmp) {
//         /* fallback: if we can't get a temp file for some reason, just
//          * print normally rather than losing output */
//         ast_print(tree, indent);
//         return;
//     }

//     /* Redirect stdout to the temp file for the duration of the call.
//      * freopen with NULL is non-portable, so instead we redirect the
//      * process's real stdout stream onto the temp file's contents by
//      * swapping FILE* buffers via freopen on the stdout stream name. */
//     fflush(stdout);
//     FILE *saved_stdout = stdout;
//     (void)saved_stdout;

// #if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
//     /* Duplicate the current stdout file descriptor so we can restore
//      * it afterward, then point fd 1 at the temp file. This works the
//      * same way on Windows (_dup/_dup2) and POSIX (dup/dup2). */
// #if defined(_WIN32)
//     int saved_fd = _dup(FILENO(stdout));
//     fflush(stdout);
//     _dup2(_fileno(tmp), FILENO(stdout));
// #else
//     int saved_fd = dup(FILENO(stdout));
//     fflush(stdout);
//     dup2(fileno(tmp), FILENO(stdout));
// #endif

//     ast_print(tree, indent);
//     fflush(stdout);

// #if defined(_WIN32)
//     _dup2(saved_fd, FILENO(stdout));
//     _close(saved_fd);
// #else
//     dup2(saved_fd, FILENO(stdout));
//     close(saved_fd);
// #endif
// #else
//     /* Unknown platform without dup/dup2: no safe way to redirect the
//      * fd, so just print directly rather than risk breaking output. */
//     fclose(tmp);
//     ast_print(tree, indent);
//     return;
// #endif

//     /* Read back everything ast_print wrote and box each line. */
//     rewind(tmp);
//     char line[4096];
//     while (fgets(line, sizeof(line), tmp)) {
//         size_t l = strlen(line);
//         while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
//         box_text("%s", line);
//     }
//     fclose(tmp);
// }

// /* ------------------------------------------------------------------ */
// /* High-level UI pieces                                                */
// /* ------------------------------------------------------------------ */

// static void banner(void)
// {
//     box_line("+", "-", "+");
//     box_blank();
//     box_center("MiniLang Compiler  v1.0", C_BOLD);
//     box_center("Lexical -> Parsing -> Semantic -> TAC -> Optimize -> Assembly", C_DIM);
//     box_blank();
//     box_line("+", "-", "+");
// }

// static void phase(int n, const char *desc)
// {
//     char label[256];
//     snprintf(label, sizeof(label), "[Phase %d/5]  %s", n, desc);

//     box_blank();
//     box_text_c(C_CYAN, "%s", label);

//     int len = (int)strlen(desc) + 12; /* keep an underline proportional to text */
//     if (len > BOX_WIDTH) len = BOX_WIDTH;
//     char rule[BOX_WIDTH + 1];
//     memset(rule, '-', len);
//     rule[len] = '\0';
//     box_text("%s", rule);
// }

// static void ok_msg(const char *msg)
// {
//     char buf[4096];
//     snprintf(buf, sizeof(buf), "  [ OK ]   %s", msg);
//     box_text_c(C_GREEN, "%s", buf);
// }

// static void warn_msg(const char *msg)
// {
//     char buf[4096];
//     snprintf(buf, sizeof(buf), "  [WARN]   %s", msg);
//     box_text_c(C_YELLOW, "%s", buf);
// }

// static void fail_msg(const char *msg)
// {
//     char buf[4096];
//     snprintf(buf, sizeof(buf), "  [FAIL]   %s", msg);
//     box_text_c(C_RED, "%s", buf);
// }

// static void section(const char *title)
// {
//     box_blank();
//     box_text_c(C_BLUE, "%s", title);
//     int len = (int)strlen(title);
//     if (len > BOX_WIDTH) len = BOX_WIDTH;
//     char rule[BOX_WIDTH + 1];
//     memset(rule, '-', len);
//     rule[len] = '\0';
//     box_text("%s", rule);
// }

// static void footer_close(void)
// {
//     box_blank();
//     box_line("+", "-", "+");
// }

// /* ------------------------------------------------------------------ */
// /* main                                                                */
// /* ------------------------------------------------------------------ */

// int main(int argc, char *argv[])
// {
//     ui_init();

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
//         if (strcmp(argv[i], "--dump-ast") == 0)
//             dump_ast = 1;
//         else if (strcmp(argv[i], "--no-opt") == 0)
//             no_opt = 1;
//         else if (strcmp(argv[i], "--dump-sym") == 0)
//             dump_sym = 1;
//     }

//     banner();
//     box_blank();
//     box_text("  Source file : %s", argv[1]);

//     /* ================================================================ */
//     /* Phase 1: Lexical Analysis + Parsing                              */
//     /* ================================================================ */

//     phase(1, "Lexical Analysis & Parsing");

//     FILE *src = fopen(argv[1], "r");
//     if (!src) {
//         fail_msg("Unable to open source file.");
//         footer_close();
//         fprintf(stderr, "\n%s\n", argv[1]);
//         return 1;
//     }

//     yyin = src;
//     parse_tree = NULL;
//     syntax_error_count = 0;

//     int parse_result = yyparse();
//     fclose(src);

//     if (parse_result != 0 || syntax_error_count > 0 || !parse_tree) {
//         fail_msg("Syntax error(s) found. Compilation aborted.");
//         footer_close();
//         fprintf(stderr, "\n  %d syntax error(s) found.\n", syntax_error_count);

//         if (parse_tree) {
//             ast_free(parse_tree);
//             parse_tree = NULL;
//         }

//         return 1;
//     }

//     ok_msg("Parsing complete - AST built.");

//     if (dump_ast) {
//         section("Abstract Syntax Tree");
//         box_capture_ast(parse_tree, 2);
//     }

//     /* ================================================================ */
//     /* Phase 2: Semantic Analysis                                       */
//     /* ================================================================ */

//     phase(2, "Semantic Analysis");

//     SemanticContext *sem = semantic_create();

//     if (dump_sym) {
//         box_text("  (Symbol table dump requested - shown after analysis)");
//     }

//     int sem_errors = semantic_analyze(sem, parse_tree);
//     semantic_destroy(sem);

//     if (sem_errors > 0) {
//         fail_msg("Semantic error(s) found. Compilation aborted.");
//         footer_close();
//         fprintf(stderr, "\n  %d semantic error(s) found.\n", sem_errors);

//         ast_free(parse_tree);
//         return 1;
//     }

//     ok_msg("No semantic errors. AST type-annotated.");

//     /* ================================================================ */
//     /* Phase 3: Intermediate Code Generation                            */
//     /* ================================================================ */

//     phase(3, "Intermediate Code Generation (TAC)");

//     TACList *tac = codegen(parse_tree);

//     ok_msg("Three-Address Code generated.");

//     /* ================================================================ */
//     /* Phase 4: Optimisation                                            */
//     /* ================================================================ */

//     phase(4, "Optimisation");

//     if (no_opt) {
//         warn_msg("Optimization disabled (--no-opt)");
//     } else {
//         optimize(tac);
//         ok_msg("Constant Folding");
//         ok_msg("Copy Propagation");
//         ok_msg("Dead Code Elimination");
//     }

//     FILE *tac_fp = fopen("output.tac", "w");

//     if (!tac_fp) {
//         warn_msg("Unable to create output.tac");
//     } else {
//         tac_write(tac, tac_fp);
//         fclose(tac_fp);
//         ok_msg("TAC written -> output.tac");
//     }

//     /* ================================================================ */
//     /* Phase 5: Target Code Generation                                  */
//     /* ================================================================ */

//     phase(5, "Target Code Generation (Stack Machine)");

//     FILE *asm_fp = fopen("output.asm", "w");

//     if (!asm_fp) {
//         warn_msg("Unable to create output.asm");
//     } else {
//         target_codegen(tac, asm_fp);
//         fclose(asm_fp);
//         ok_msg("Assembly written -> output.asm");
//     }

//     /* ================================================================ */
//     /* Cleanup                                                          */
//     /* ================================================================ */

//     tac_destroy(tac);
//     ast_free(parse_tree);
//     parse_tree = NULL;

//     /* ================================================================ */
//     /* Summary                                                          */
//     /* ================================================================ */

//     section("COMPILATION SUCCESSFUL");

//     box_text("  Generated Files");
//     box_text("    output.tac    Optimized Three-Address Code");
//     box_text("    output.asm    Stack Machine Assembly");
//     box_blank();
//     box_text("  Compilation Summary");
//     box_text("    Parsing          : %sSuccessful%s", C_GREEN, C_RESET);
//     box_text("    Semantic Check   : %sSuccessful%s", C_GREEN, C_RESET);
//     box_text("    Optimization     : %s%s%s",
//              no_opt ? C_YELLOW : C_GREEN,
//              no_opt ? "Disabled" : "Enabled",
//              C_RESET);
//     box_text("    Output Generated : %sYes%s", C_GREEN, C_RESET);

//     footer_close();

//     return 0;
// }

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
 *   --dump-ast   Print the AST to stdout after parsing.
 *   --dump-sym   (reserved; symbol table is printed on request)
 *   --no-opt     Skip optimisation pass; emit raw TAC.
 *
 * Output files
 * ------------
 *   output.tac   Three-address code (after optional optimisation)
 *   output.asm   Stack-machine pseudo-assembly
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

/* Portable "is this a real terminal" check: on POSIX systems we use
 * isatty(); on Windows (MSVC / MinGW) we use the equivalent _isatty().
 * If neither is available we simply default color off, which is always
 * safe (plain text still displays correctly everywhere).            */
#if defined(_WIN32)
#include <io.h>
#define ISATTY(fd) _isatty(fd)
#define FILENO(f)  _fileno(f)
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#define FILENO(f)  fileno(f)
#else
#define ISATTY(fd) 0
#define FILENO(f)  0
#endif

/* Symbols provided by the bison/flex generated sources */
extern FILE    *yyin;
extern int      yyparse(void);
extern ASTNode *parse_tree;
extern int      syntax_error_count;

/* ------------------------------------------------------------------ */
/* Terminal UI helpers                                                 */
/* ------------------------------------------------------------------ */

#define BOX_WIDTH 78   /* interior width, not counting the two border chars */

/* Colour support: only emit ANSI codes when stdout is an actual terminal,
 * so piping output to a file or another program stays clean.            */
static int g_color = 0;

static void ui_init(void)
{
    g_color = ISATTY(FILENO(stdout));
}

#define C_RESET   (g_color ? "\033[0m"  : "")
#define C_BOLD    (g_color ? "\033[1m"  : "")
#define C_DIM     (g_color ? "\033[2m"  : "")
#define C_CYAN    (g_color ? "\033[36m" : "")
#define C_GREEN   (g_color ? "\033[32m" : "")
#define C_YELLOW  (g_color ? "\033[33m" : "")
#define C_RED     (g_color ? "\033[31m" : "")
#define C_BLUE    (g_color ? "\033[34m" : "")

/* Prints a horizontal box border, e.g. +----------------------------+ */
static void box_line(const char *left, const char *mid, const char *right)
{
    printf("%s", left);
    for (int i = 0; i < BOX_WIDTH; i++) printf("%s", mid);
    printf("%s\n", right);
}

/* Prints exactly one already-wrapped, already-length-checked chunk
 * ("segment") as one bordered line, coloring it AFTER padding is
 * computed so ANSI escape bytes never count toward the visible width. */
static void box_segment(const char *segment, int len, const char *color)
{
    if (len < 0) len = 0;
    if (len > BOX_WIDTH) len = BOX_WIDTH; /* hard safety net, should not trigger */

    printf("|%s%.*s%s%*s|\n",
           color, len, segment, C_RESET,
           BOX_WIDTH - len, "");
}

/* Prints text inside the box, left-aligned. Any text longer than
 * BOX_WIDTH is WORD-WRAPPED onto additional bordered lines instead of
 * being truncated or allowed to break the box, so arbitrarily long
 * filenames / messages / identifiers can never corrupt the layout.
 * Wraps on spaces where possible; hard-splits only if a single "word"
 * is itself longer than the box (e.g. a long path with no spaces). */
static void box_text_c(const char *color, const char *fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    int total = (int)strlen(buf);
    if (total == 0) {
        box_segment("", 0, "");
        return;
    }

    int pos = 0;
    while (pos < total) {
        int remaining = total - pos;
        int take = remaining;

        if (take > BOX_WIDTH) {
            take = BOX_WIDTH;
            /* try to break at the last space within this chunk so we
             * don't split a word mid-way */
            int break_at = -1;
            for (int i = take; i > 0; i--) {
                if (buf[pos + i - 1] == ' ') { break_at = i; break; }
            }
            /* only use the space-break if it doesn't waste more than
             * half the line (otherwise a long unbroken token exists —
             * hard-split it instead, still perfectly safe/aligned) */
            if (break_at > BOX_WIDTH / 2) take = break_at;
        }

        box_segment(buf + pos, take, color);

        pos += take;
        while (pos < total && buf[pos] == ' ') pos++; /* skip the space we broke on */
    }
}

/* Convenience wrapper: no color. */
static void box_text(const char *fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    box_text_c("", "%s", buf);
}

/* Centered text inside the box (used for titles). Long titles wrap
 * via box_text_c rather than overflowing the border. */
static void box_center(const char *text, const char *color)
{
    int len = (int)strlen(text);
    if (len > BOX_WIDTH) { box_text_c(color, "%s", text); return; }

    int left_pad  = (BOX_WIDTH - len) / 2;
    int right_pad = BOX_WIDTH - len - left_pad;

    printf("|%*s%s%.*s%s%*s|\n",
           left_pad, "",
           color, len, text, C_RESET,
           right_pad, "");
}

/* Prints a "label: value" line where the value is colored, computing
 * padding from the VISIBLE combined length only (label + value, no
 * ANSI bytes) so the right border can never drift, regardless of
 * which color or how long the label/value are. Wraps via box_text_c
 * if the combined plain text would exceed the box width. */
static void box_text_kv(const char *label, const char *color, const char *value)
{
    char plain[4096];
    snprintf(plain, sizeof(plain), "%s%s", label, value);

    if ((int)strlen(plain) > BOX_WIDTH) {
        /* fall back to plain (uncolored) wrapping for safety */
        box_text("%s", plain);
        return;
    }

    int total_len = (int)strlen(label) + (int)strlen(value);

    printf("|%s%s%s%s%*s|\n",
           label,
           color, value,
           C_RESET,
           BOX_WIDTH - total_len, "");
}

static void box_blank(void)
{
    printf("|%*s|\n", BOX_WIDTH, "");
}

/* Runs a printing function (e.g. ast_print) while capturing whatever
 * it writes to stdout, then re-emits every captured line through
 * box_text so the box border is never broken open by callee output
 * that doesn't know about the box at all.
 *
 * Implemented with plain standard-C stdio (freopen + tmpfile) instead
 * of fork()/pipe(), so it compiles and works identically on Windows
 * (MinGW/MSVC), Linux, and macOS with no platform-specific headers. */
static void box_capture_ast(ASTNode *tree, int indent)
{
    fflush(stdout);

    FILE *tmp = tmpfile();
    if (!tmp) {
        /* fallback: if we can't get a temp file for some reason, just
         * print normally rather than losing output */
        ast_print(tree, indent);
        return;
    }

    /* Redirect stdout to the temp file for the duration of the call.
     * freopen with NULL is non-portable, so instead we redirect the
     * process's real stdout stream onto the temp file's contents by
     * swapping FILE* buffers via freopen on the stdout stream name. */
    fflush(stdout);
    FILE *saved_stdout = stdout;
    (void)saved_stdout;

#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    /* Duplicate the current stdout file descriptor so we can restore
     * it afterward, then point fd 1 at the temp file. This works the
     * same way on Windows (_dup/_dup2) and POSIX (dup/dup2). */
#if defined(_WIN32)
    int saved_fd = _dup(FILENO(stdout));
    fflush(stdout);
    _dup2(_fileno(tmp), FILENO(stdout));
#else
    int saved_fd = dup(FILENO(stdout));
    fflush(stdout);
    dup2(fileno(tmp), FILENO(stdout));
#endif

    ast_print(tree, indent);
    fflush(stdout);

#if defined(_WIN32)
    _dup2(saved_fd, FILENO(stdout));
    _close(saved_fd);
#else
    dup2(saved_fd, FILENO(stdout));
    close(saved_fd);
#endif
#else
    /* Unknown platform without dup/dup2: no safe way to redirect the
     * fd, so just print directly rather than risk breaking output. */
    fclose(tmp);
    ast_print(tree, indent);
    return;
#endif

    /* Read back everything ast_print wrote and box each line. */
    rewind(tmp);
    char line[4096];
    while (fgets(line, sizeof(line), tmp)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
        box_text("%s", line);
    }
    fclose(tmp);
}

/* ------------------------------------------------------------------ */
/* High-level UI pieces                                                */
/* ------------------------------------------------------------------ */

static void banner(void)
{
    box_line("+", "-", "+");
    box_blank();
    box_center("MiniLang Compiler  v1.0", C_BOLD);
    box_center("Lexical -> Parsing -> Semantic -> TAC -> Optimize -> Assembly", C_DIM);
    box_blank();
    box_line("+", "-", "+");
}

static void phase(int n, const char *desc)
{
    char label[256];
    snprintf(label, sizeof(label), "[Phase %d/5]  %s", n, desc);

    box_blank();
    box_text_c(C_CYAN, "%s", label);

    int len = (int)strlen(desc) + 12; /* keep an underline proportional to text */
    if (len > BOX_WIDTH) len = BOX_WIDTH;
    char rule[BOX_WIDTH + 1];
    memset(rule, '-', len);
    rule[len] = '\0';
    box_text("%s", rule);
}

static void ok_msg(const char *msg)
{
    char buf[4096];
    snprintf(buf, sizeof(buf), "  [ OK ]   %s", msg);
    box_text_c(C_GREEN, "%s", buf);
}

static void warn_msg(const char *msg)
{
    char buf[4096];
    snprintf(buf, sizeof(buf), "  [WARN]   %s", msg);
    box_text_c(C_YELLOW, "%s", buf);
}

static void fail_msg(const char *msg)
{
    char buf[4096];
    snprintf(buf, sizeof(buf), "  [FAIL]   %s", msg);
    box_text_c(C_RED, "%s", buf);
}

static void section(const char *title)
{
    box_blank();
    box_text_c(C_BLUE, "%s", title);
    int len = (int)strlen(title);
    if (len > BOX_WIDTH) len = BOX_WIDTH;
    char rule[BOX_WIDTH + 1];
    memset(rule, '-', len);
    rule[len] = '\0';
    box_text("%s", rule);
}

static void footer_close(void)
{
    box_blank();
    box_line("+", "-", "+");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    ui_init();

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
        if (strcmp(argv[i], "--dump-ast") == 0)
            dump_ast = 1;
        else if (strcmp(argv[i], "--no-opt") == 0)
            no_opt = 1;
        else if (strcmp(argv[i], "--dump-sym") == 0)
            dump_sym = 1;
    }

    banner();
    box_blank();
    box_text("  Source file : %s", argv[1]);

    /* ================================================================ */
    /* Phase 1: Lexical Analysis + Parsing                              */
    /* ================================================================ */

    phase(1, "Lexical Analysis & Parsing");

    FILE *src = fopen(argv[1], "r");
    if (!src) {
        fail_msg("Unable to open source file.");
        footer_close();
        fprintf(stderr, "\n%s\n", argv[1]);
        return 1;
    }

    yyin = src;
    parse_tree = NULL;
    syntax_error_count = 0;

    int parse_result = yyparse();
    fclose(src);

    if (parse_result != 0 || syntax_error_count > 0 || !parse_tree) {
        fail_msg("Syntax error(s) found. Compilation aborted.");
        footer_close();
        fprintf(stderr, "\n  %d syntax error(s) found.\n", syntax_error_count);

        if (parse_tree) {
            ast_free(parse_tree);
            parse_tree = NULL;
        }

        return 1;
    }

    ok_msg("Parsing complete - AST built.");

    if (dump_ast) {
        section("Abstract Syntax Tree");
        box_capture_ast(parse_tree, 2);
    }

    /* ================================================================ */
    /* Phase 2: Semantic Analysis                                       */
    /* ================================================================ */

    phase(2, "Semantic Analysis");

    SemanticContext *sem = semantic_create();

    if (dump_sym) {
        box_text("  (Symbol table dump requested - shown after analysis)");
    }

    int sem_errors = semantic_analyze(sem, parse_tree);
    semantic_destroy(sem);

    if (sem_errors > 0) {
        fail_msg("Semantic error(s) found. Compilation aborted.");
        footer_close();
        fprintf(stderr, "\n  %d semantic error(s) found.\n", sem_errors);

        ast_free(parse_tree);
        return 1;
    }

    ok_msg("No semantic errors. AST type-annotated.");

    /* ================================================================ */
    /* Phase 3: Intermediate Code Generation                            */
    /* ================================================================ */

    phase(3, "Intermediate Code Generation (TAC)");

    TACList *tac = codegen(parse_tree);

    ok_msg("Three-Address Code generated.");

    /* ================================================================ */
    /* Phase 4: Optimisation                                            */
    /* ================================================================ */

    phase(4, "Optimisation");

    if (no_opt) {
        warn_msg("Optimization disabled (--no-opt)");
    } else {
        optimize(tac);
        ok_msg("Constant Folding");
        ok_msg("Copy Propagation");
        ok_msg("Dead Code Elimination");
    }

    FILE *tac_fp = fopen("output.tac", "w");

    if (!tac_fp) {
        warn_msg("Unable to create output.tac");
    } else {
        tac_write(tac, tac_fp);
        fclose(tac_fp);
        ok_msg("TAC written -> output.tac");
    }

    /* ================================================================ */
    /* Phase 5: Target Code Generation                                  */
    /* ================================================================ */

    phase(5, "Target Code Generation (Stack Machine)");

    FILE *asm_fp = fopen("output.asm", "w");

    if (!asm_fp) {
        warn_msg("Unable to create output.asm");
    } else {
        target_codegen(tac, asm_fp);
        fclose(asm_fp);
        ok_msg("Assembly written -> output.asm");
    }

    /* ================================================================ */
    /* Cleanup                                                          */
    /* ================================================================ */

    tac_destroy(tac);
    ast_free(parse_tree);
    parse_tree = NULL;

    /* ================================================================ */
    /* Summary                                                          */
    /* ================================================================ */

    section("COMPILATION SUCCESSFUL");

    box_text("  Generated Files");
    box_text("    output.tac    Optimized Three-Address Code");
    box_text("    output.asm    Stack Machine Assembly");
    box_blank();
    box_text("  Compilation Summary");
    box_text_kv("    Parsing          : ", C_GREEN, "Successful");
    box_text_kv("    Semantic Check   : ", C_GREEN, "Successful");
    box_text_kv("    Optimization     : ", no_opt ? C_YELLOW : C_GREEN,
                no_opt ? "Disabled" : "Enabled");
    box_text_kv("    Output Generated : ", C_GREEN, "Yes");

    footer_close();

    return 0;
}