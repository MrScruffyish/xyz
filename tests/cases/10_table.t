=== one column
table(x^2, x = 1 to 5)
---
1
4
9
16
25

=== two columns
table(x, x^2, x = 1 to 3)
---
1	1
2	4
3	9

=== nested ranges, first one outermost
table(x, y, x*y, x = 1 to 2, y = 1 to 3)
---
1	1	1
1	2	2
1	3	3
2	1	2
2	2	4
2	3	6

=== steps and fractions
table(t, 2t, t = 0 to 1 step 1/3)
table(n, n = 3 to 1 step -1)
---
0	0
1/3	2/3
2/3	4/3
1	2
3
2
1

=== range variables are temporary and plug into snapshots
x = 10;
p = x2^2+1;
table(p, x2 = 1 to 2)
x
---
2
5
10

=== symbolic results
table(expand((x+a)^2), a = 0 to 1)
---
x^2
x^2+2*x+1

=== table errors
table(x, x = 1)
table(x, x = 1 to y)
table(x = 1 to 2)
1 + table(x, x = 1 to 2)
eval(x, x = 1 to 2)
---
error: line 1: table: write a range as x = start to stop
error: line 2: the end of a range must be a number, not y
error: line 3: table: nothing to show
error: line 4: table() can only be used as a statement on its own
error: line 5: eval: ranges (x = a to b) are only allowed in table(), seq() and sum()
