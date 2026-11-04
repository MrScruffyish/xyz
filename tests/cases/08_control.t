=== if, else if, else
s = 5;
if (s < 0) { disp(-1) } else if (s == 0) { disp(0) } else { disp(1) }
s = 0;
if (s < 0) { disp(-1) } else if (s == 0) { disp(0) } else { disp(1) }
s = -2;
if (s < 0) { disp(-1) } else if (s == 0) { disp(0) } else { disp(1) }
if (s > 0) { disp(1) }
---
1
0
-1

=== else on the next line, blocks over several lines
x = 3;
if (x > 5) {
  disp(big)
}
else if (x > 1) {
  disp(medium)
}
else {
  disp(small)
}
---
medium

=== while
n = 1;
while (n < 100) { n = 2n; }
n
---
128

=== while that never runs
n = 5;
while (n < 0) { disp(never) }
n
---
5

=== do-while runs at least once
n = 5;
do { n = n+1; } while (n < 0)
n
do {
  n = n-2;
} while (n > 0)
n
---
6
0

=== for loops
for (i = 1 to 5) { disp(i^2) }
---
1
4
9
16
25

=== for with steps
for (i = 10 to 1 step -3) { disp(i) }
for (t = 0 to 1 step 1/4) { disp(t) }
for (i = 1 to 2 step 3/4) { disp(i) }
---
10
7
4
1
0
1/4
1/2
3/4
1
1
7/4

=== for that never runs
for (i = 5 to 1) { disp(i) }
for (i = 1 to 5 step -1) { disp(i) }
disp(done)
---
done

=== for errors
for (i = 1 to 5 step 0) { }
for (i = 1 to x) { }
for (i = 1 to 3) { i = 5; }
---
error: line 1: the step of a for loop cannot be 0
error: line 2: the end of a for loop must be a number, not x
error: line 3: cannot assign to the loop variable 'i'

=== the loop variable is separate from a variable of the same name
i = 2;
for (i = 10 to 20) { }
disp(i)
s = 0;
for (i = 1 to 4) { s = s+i; }
s
i
---
2
10
2

=== nested loops
for (i = 1 to 3) { for (j = 1 to i) { disp(i, j) } }
---
1	1
2	1
2	2
3	1
3	2
3	3

=== a formula that mentions the loop variable is ambiguous
a := i^2;
for (i = 1 to 3) { disp(a) }
for (i = 1 to 3) { disp(eval(a, i=i)) }
f(k) := k^2;
for (i = 1 to 3) { disp(f(i)) }
---
error: line 2: a refers to i, which is also the loop variable here; use eval(a, i=i) for the loop value, or rename the loop variable
1
4
9
1
4
9

=== a definition made inside a loop captures the loop value
for (i = 1 to 3) { a := x+i; }
a
---
x+3

=== errors inside a loop report the inner line and stop the loop
for (i = -1 to 1) {
  disp(1/i)
}
disp(after)
---
-1
error: line 2: division by zero
after

=== quit inside a loop
for (i = 1 to 10) { disp(i); if (i == 2) { quit } }
disp(never)
---
1
2

=== conditions must be numbers
if (x) { }
while (y > 1) { }
---
error: line 1: condition is not a number: x
error: line 2: cannot compare y > 1: both sides must be numbers

=== missing braces
if (1) disp(1)
for (i = 1 to 2) disp(i)
---
error: line 1: syntax error: expected '{', found 'disp'
error: line 2: syntax error: expected '{', found 'disp'

=== unclosed block
if (1) { disp(1)
---
error: line 1: missing '}' for the block opened on line 1

=== the '{' may be on the next line
a = 2;
if (a == 2)
{ b = 3; }
else
{ b = 10; }
b
while (a < 5)
{ a = a+1; }
a
---
3
5

=== an else in a later statement continues the if just executed
a = 2;
if (a == 2) { b = 3; }; else { b = 10; }
b
a = 7;
if (a == 2) { b = 3; }; else if (a == 5) { b = 5; }; else { b = 10; }
b
---
3
10

=== else without if
else { disp(1) }
x = 1; else { disp(1) }
if (1) { } else { }; else { disp(2) }
---
error: line 1: 'else' without a preceding 'if'
error: line 2: 'else' without a preceding 'if'
error: line 3: 'else' without a preceding 'if'

=== C-style for loop
for (i = 1; i*i < 10*i; i = i+1) { disp(i) }
---
1
2
3
4
5
6
7
8
9

=== C-style for with any step, loop variable is local
i = 100;
s = 0;
for (i = 1; i <= 64; i = 2i) { s = s+i; }
s
i
for (t = 1; t > 1/10; t = t/3) { disp(t) }
---
127
100
1
1/3
1/9

=== C-style for errors
for (i = 1; i < 3; j = i+1) { }
for (i = 1; x; i = i+1) { }
for (i = 1; i < 3; i = i+1) { i = 7; }
---
error: line 1: syntax error: the step of a for loop must assign the loop variable i, found 'j'
error: line 2: condition is not a number: x
error: line 3: cannot assign to the loop variable 'i'
