/* test7.ml – Unary minus and chained conditions */

int x;
int y;

x = -5;          /* unary minus lowers to  0 - 5 */
y = -x;          /* y = 0 - (-5) = 5              */

print(x);        /* expected: -5 */
print(y);        /* expected:  5 */

bool neg;
neg = x < 0;

if (neg) {
    int abs_x;
    abs_x = 0 - x;
    print(abs_x);  /* expected: 5 */
} else {
    print(x);
}
