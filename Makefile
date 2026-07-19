CC = gcc
FLEX = flex
BISON = bison

TARGET = minicompiler
LEX_SRC = lexer.l
YACC_SRC = parser.y

SOURCES = lex.yy.c parser.tab.c ast.c main.c symbol_table.c semantic.c codegen.c
OBJECTS = lex.yy.c parser.tab.c ast.c main.c symbol_table.c semantic.c codegen.c

all: $(TARGET)

$(TARGET): lex.yy.c parser.tab.c
	$(CC) lex.yy.c parser.tab.c ast.c main.c symbol_table.c semantic.c codegen.c -o $(TARGET)

lex.yy.c: $(LEX_SRC)
	$(FLEX) $(LEX_SRC)

parser.tab.c parser.tab.h: $(YACC_SRC)
	$(BISON) -d $(YACC_SRC)

clean:
	rm -f lex.yy.c parser.tab.c parser.tab.h $(TARGET) output.tac output.asm

.PHONY: all clean