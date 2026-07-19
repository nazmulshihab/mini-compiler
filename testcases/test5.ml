/* test5.ml – Constant folding and algebraic simplification
   After optimisation the compiler should fold everything to
   a single constant assignment with no live temporaries.    */

int a;
int b;
int c;

a = 3 + 4;          /* folded → 7          */
b = a * 2;          /* folded → 14 (after propagation) */
c = b - 0;          /* algebraic: x-0 → x  */
c = c * 1;          /* algebraic: x*1 → x  */
c = 0 + c;          /* algebraic: 0+x → x  */

print(c);           /* expected: 14 */

/* Nested folding */
int d;
d = (2 + 3) * (10 - 6);   /* (5)*(4) → 20 */
print(d);           /* expected: 20 */
