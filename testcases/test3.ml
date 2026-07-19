/* test3.ml – while loop: sum 1..10 */

int i;
int sum;

i   = 1;
sum = 0;

while (i < 11) {
    sum = sum + i;
    i   = i + 1;
}

print(sum);   /* expected: 55 */
print(i);     /* expected: 11 */
