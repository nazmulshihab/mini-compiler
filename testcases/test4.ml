/* test4.ml – Block scoping and variable shadowing */

int x;
x = 1;

{
    int x;      /* shadows outer x inside this block */
    x = 99;
    print(x);   /* expected: 99  (inner x) */
}

print(x);       /* expected: 1   (outer x unchanged) */

{
    int y;
    y = x + 10;
    print(y);   /* expected: 11 */
}
