/*
 * parser.y  –  Bison LALR(1) parser for MiniLang.
 *
 * Grammar overview
 * ----------------
 *   program   → stmt*
 *   stmt      → decl | assign | if_stmt | while_stmt | print_stmt | block
 *   decl      → ('int'|'bool') IDENTIFIER ';'
 *   assign    → IDENTIFIER '=' expr ';'
 *   if_stmt   → 'if' '(' expr ')' block  [ 'else' block ]
 *   while_stmt→ 'while' '(' expr ')' block
 *   print_stmt→ 'print' '(' expr ')' ';'
 *   block     → '{' stmt* '}'
 *   expr      → expr ('+' | '-' | '*' | '/' | '<' | '>' | '==' | '!=') expr
 *             | '(' expr ')'
 *             | '-' expr              %prec UMINUS
 *             | IDENTIFIER | INT_CONST | BOOL_CONST
 *
 * Operator precedence (lowest → highest):
 *   left   <  >  ==  !=
 *   left   +  -
 *   left   *  /
 *   right  UMINUS
 */

%code requires {
    #include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* Provided by the flex-generated scanner */
extern int  yylineno;
extern int  yylex(void);
extern FILE *yyin;

void yyerror(const char *msg);

/* The AST root; set in the 'program' action and read by main.c */
ASTNode *parse_tree = NULL;

/* Count syntax errors so main.c can abort after parsing */
int syntax_error_count = 0;
%}

/* ------------------------------------------------------------------ */
/* Semantic-value union                                                */
/* ------------------------------------------------------------------ */
%union {
    int      ival;      /* INT_CONST, BOOL_CONST */
    char    *sval;      /* IDENTIFIER            */
    ASTNode *node;      /* AST nodes             */
    DataType dtype;     /* type non-terminal     */
}

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */
%token <ival>  INT_CONST
%token <ival>  BOOL_CONST
%token <sval>  IDENTIFIER

%token  INT_KW BOOL_KW
%token  IF ELSE WHILE PRINT
%token  PLUS MINUS STAR SLASH
%token  LT GT EQ NEQ
%token  ASSIGN
%token  LPAREN RPAREN
%token  LBRACE RBRACE
%token  SEMICOLON

/* ------------------------------------------------------------------ */
/* Non-terminal types                                                  */
/* ------------------------------------------------------------------ */
%type <node>  program stmt_list stmt
%type <node>  decl_stmt assign_stmt if_stmt while_stmt print_stmt block
%type <node>  expr
%type <dtype> type

/* ------------------------------------------------------------------ */
/* Operator precedence                                                 */
/* ------------------------------------------------------------------ */
%left  LT GT EQ NEQ
%left  PLUS MINUS
%left  STAR SLASH
%right UMINUS

/* ------------------------------------------------------------------ */
/* Verbose error messages                                              */
/* ------------------------------------------------------------------ */
%define parse.error verbose

%%

program
    : stmt_list
        {
            $1->type = NODE_PROGRAM;
            parse_tree = $1;
        }
    ;

stmt_list
    : /* empty */
        {
            $$ = ast_new_block(yylineno);
        }
    | stmt_list stmt
        {
            $$ = $1;
            if ($2) ast_add_stmt($$, $2);
        }
    ;

stmt
    : decl_stmt     { $$ = $1; }
    | assign_stmt   { $$ = $1; }
    | if_stmt       { $$ = $1; }
    | while_stmt    { $$ = $1; }
    | print_stmt    { $$ = $1; }
    | block         { $$ = $1; }
    | error SEMICOLON
        {
            $$ = NULL;
            yyerrok;
        }
    ;

decl_stmt
    : type IDENTIFIER SEMICOLON
        {
            $$ = ast_new_decl($1, $2, yylineno);
            free($2);
        }
    ;

assign_stmt
    : IDENTIFIER ASSIGN expr SEMICOLON
        {
            $$ = ast_new_assign($1, $3, yylineno);
            free($1);
        }
    ;

if_stmt
    : IF LPAREN expr RPAREN block
        {
            $$ = ast_new_if($3, $5, NULL, yylineno);
        }
    | IF LPAREN expr RPAREN block ELSE block
        {
            $$ = ast_new_if($3, $5, $7, yylineno);
        }
    ;

while_stmt
    : WHILE LPAREN expr RPAREN block
        {
            $$ = ast_new_while($3, $5, yylineno);
        }
    ;

print_stmt
    : PRINT LPAREN expr RPAREN SEMICOLON
        {
            $$ = ast_new_print($3, yylineno);
        }
    ;

block
    : LBRACE stmt_list RBRACE
        {
            $$ = $2;
        }
    ;

type
    : INT_KW    { $$ = TYPE_INT;  }
    | BOOL_KW   { $$ = TYPE_BOOL; }
    ;

expr
    : expr PLUS  expr   { $$ = ast_new_binop("+",  $1, $3, yylineno); }
    | expr MINUS expr   { $$ = ast_new_binop("-",  $1, $3, yylineno); }
    | expr STAR  expr   { $$ = ast_new_binop("*",  $1, $3, yylineno); }
    | expr SLASH expr   { $$ = ast_new_binop("/",  $1, $3, yylineno); }
    | expr LT    expr   { $$ = ast_new_binop("<",  $1, $3, yylineno); }
    | expr GT    expr   { $$ = ast_new_binop(">",  $1, $3, yylineno); }
    | expr EQ    expr   { $$ = ast_new_binop("==", $1, $3, yylineno); }
    | expr NEQ   expr   { $$ = ast_new_binop("!=", $1, $3, yylineno); }

    | MINUS expr  %prec UMINUS
        {
            $$ = ast_new_binop("-",
                               ast_new_int_const(0, yylineno),
                               $2, yylineno);
        }

    | LPAREN expr RPAREN { $$ = $2; }

    | IDENTIFIER
        {
            $$ = ast_new_var($1, yylineno);
            free($1);
        }
    | INT_CONST
        {
            $$ = ast_new_int_const($1, yylineno);
        }
    | BOOL_CONST
        {
            $$ = ast_new_bool_const($1, yylineno);
        }
    ;

%%

void yyerror(const char *msg)
{
    syntax_error_count++;
    fprintf(stderr, "Syntax Error [line %d]: %s\n", yylineno, msg);
}