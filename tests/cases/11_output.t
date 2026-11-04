# A statement prints its result unless it ends with ';'.
=== inside blocks the same rule applies
for (i = 1 to 2) { s = i^2 }
for (i = 1 to 2) { s = i^2; }
for (i = 1 to 2) { i+10 }
s
---
s = 1
s = 4
11
12
4

=== statements before a newline or '}' are shown
if (1) { a = 1; b = 2 }
---
b = 2

=== disp, def and table always print
disp(1);
a = 2; def(a);
table(x, x = 1 to 1);
---
1
a = 2
1

=== unset prints nothing
a = 1;
unset(a)
---

=== num results can be shown and stored, but not computed with
a = num(1/3, 5)
a
a+1
---
a = 0.33333
0.33333
error: line 3: a num() result cannot be used in further calculations
