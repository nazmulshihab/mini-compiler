/* test2.ml – if / else and bool type */

int x;
bool flag;

x = 42;
flag = x > 10;

if (flag) {
    print(x);       /* expected: 42 */
} else {
    print(0);
}

if (x == 100) {
    print(1);
} else {
    print(0);       /* expected: 0  */
}
