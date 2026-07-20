; ============================================
;  Three-Address Code  –  MiniLang Compiler
; ============================================

    x = -5
    t6 = 0 - x
    y = t6
    print x
    print y
    t11 = x < 0
    neg = t11
    ifFalse neg goto L1
    t15 = 0 - x
    abs_x = t15
    print abs_x
    goto L2
L1:
    print x
L2:

