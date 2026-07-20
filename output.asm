; ============================================
;  Target Code (Stack Machine Pseudo-Assembly)
;  MiniLang Compiler
; ============================================
;
;  PUSH <imm>  – push integer constant
;  LOAD <var>  – load variable onto stack
;  POP  <var>  – pop stack into variable
;  ADD SUB MUL DIV  – arithmetic
;  LT  GT  EQ  NEQ – comparison (result: 0 or 1)
;  JMP <label> – unconditional branch
;  JZ  <label> – branch if top-of-stack == 0
;  PRINT        – pop and output
;  HALT         – terminate
;

    PUSH -5
    POP  x
    PUSH 0
    LOAD x
    SUB
    POP  t6
    LOAD t6
    POP  y
    LOAD x
    PRINT
    LOAD y
    PRINT
    LOAD x
    PUSH 0
    LT
    POP  t11
    LOAD t11
    POP  neg
    LOAD neg
    JZ  L1
    PUSH 0
    LOAD x
    SUB
    POP  t15
    LOAD t15
    POP  abs_x
    LOAD abs_x
    PRINT
    JMP L2
L1:
    LOAD x
    PRINT
L2:
    HALT
