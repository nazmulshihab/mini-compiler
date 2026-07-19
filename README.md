# MiniCompiler

MiniLang compiler project for CSE 712 Compiler Lab.

Build:
```bash
flex lexer.l
bison -d parser.y
gcc lex.yy.c parser.tab.c ast.c main.c symbol_table.c semantic.c codegen.c -o minicompiler
```

Or use:
```bash
make
```

Run:
```bash
./minicompiler testcases/sample.ml
```
