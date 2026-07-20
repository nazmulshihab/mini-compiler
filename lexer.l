%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

/* Count lexical errors so main.c can report/abort like it does for
   syntax and semantic errors. */
int lexical_error_count = 0;
%}

%option noyywrap
%option yylineno

%x COMMENT

DIGIT   [0-9]
ALPHA   [a-zA-Z_]
ALNUM   [a-zA-Z0-9_]

%%

"int"       { return INT_KW; }
"bool"      { return BOOL_KW; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"print"     { return PRINT; }

"true"      { yylval.ival = 1; return BOOL_CONST; }
"false"     { yylval.ival = 0; return BOOL_CONST; }

{ALPHA}{ALNUM}* {
    yylval.sval = strdup(yytext);
    return IDENTIFIER;
}

{DIGIT}+ {
    yylval.ival = atoi(yytext);
    return INT_CONST;
}

"=="        { return EQ; }
"!="        { return NEQ; }

"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return STAR; }
"/"         { return SLASH; }

"<"         { return LT; }
">"         { return GT; }
"="         { return ASSIGN; }

"("         { return LPAREN; }
")"         { return RPAREN; }
"{"         { return LBRACE; }
"}"         { return RBRACE; }
";"         { return SEMICOLON; }

"//".*      { }

"/*"        { BEGIN(COMMENT); }
<COMMENT>"*/" { BEGIN(INITIAL); }
<COMMENT>.|\n { }

[ \t\r\n]+  { }

.           {
    fprintf(stderr, "Lexical Error [line %d]: unexpected character '%s'\n",
            yylineno, yytext);
    lexical_error_count++;
}

%%