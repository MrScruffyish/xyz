# subs(e, x=v): substitute first, then evaluate and simplify.
# eval(e, x=v): evaluate and simplify first (with x kept as a symbol),
#               then insert the value.
=== both agree on ordinary expressions
eval(x^2+1, x=3)
subs(x^2+1, x=3)
eval(x+y, x=1, y=2)
subs(x+y, x=1, y=2)
---
10
10
3
3

=== the order matters where a division by zero is simplified away
eval(x/x, x=0)
subs(x/x, x=0)
eval(x^3/x, x=0)
subs(x^3/x, x=0)
---
1
error: line 2: division by zero
0
error: line 4: division by zero

=== difference quotient: eval gives the derivative directly
f(x) := x^3+2x;
d := (f(x+h)-f(x))/h;
eval(expand(d), h=0)
subs(expand(d), h=0)
---
3*x^2+2
error: line 4: division by zero

=== eval cancels common factors before inserting
eval((x^2-1)/(x-1), x=1)
subs((x^2-1)/(x-1), x=1)
---
2
error: line 2: division by zero

=== the named variables are free while computing, even if assigned
x = 5;
eval(x^2, x=2)
subs(x^2, x=2)
x
---
4
4
5

=== both reach into formulas
a := x; b := a*a+3;
eval(b, a=2)
subs(b, a=2)
eval(b, x=2)
---
7
7
7

=== both plug current values into snapshots
p = (x+1)^2; x = 3;
p
eval(p)
subs(p)
eval(p, x=1)
---
(x+1)^2
16
16
4

=== substitutions happen at the same time
eval(x^2+y, x=y, y=3)
subs(x^2+y, x=y, y=3)
---
y^2+3
y^2+3

=== loop values with eval or subs
a := i^2;
for (i = 1 to 2) { disp(eval(a, i=i), subs(a, i=i)) }
---
1	1
4	4
