/* test6.ml – Comprehensive: all features in one program.
   Computes max of two numbers and a countdown.           */

int a;
int b;
int max;
bool cmp;

a = 17;
b = 25;

/* Find maximum */
cmp = a > b;
if (cmp) {
    max = a;
} else {
    max = b;
}
print(max);        /* expected: 25 */

/* Nested while with if inside */
int n;
int total;
n     = 5;
total = 0;

while (n != 0) {
    bool even;
    even = (n * 2) != (n + n + 1);   /* always true – just exercises bool */
    if (even) {
        total = total + n;
    } else {
        total = total + 0;
    }
    n = n - 1;
}

print(total);      /* expected: 15  (5+4+3+2+1) */

/* Division and relational */
int q;
q = 100 / 4;
print(q);          /* expected: 25 */
