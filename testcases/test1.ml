/* test1.ml – Basic arithmetic, variables, and print */

int a;
int b;
int c;

a = 10;
b = 3;
c = a + b * 2;   /* tests precedence: b*2 first */

print(c);        /* expected: 16 */
print(a - b);    /* expected: 7  */
