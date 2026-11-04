=== eval substitutes and evaluates
eval(x^2+1, x=3)
eval(x+y, x=1)
eval(x+y, x=1, y=2)
eval(x^2+y, x=y, y=3)
b=x^2; eval(b, x=t+1)
expand(eval(b, x=t+1))
---
10
y+1
3
y^2+3
(t+1)^2
t^2+2*t+1

=== eval overrides an assigned variable temporarily
x = 5;
eval(x^2, x=2)
x
---
4
5

=== eval of an intermediate variable
a := x; b := a*a+3;
eval(b, a=2)
b
c = a*a+3;
eval(c, a=2)
---
7
x^2+3
x^2+3

=== eval errors
eval()
eval(x, 3)
x=1+(y=2)
---
error: line 1: eval: expected 1 to 1000 arguments
error: line 2: eval: arguments after the first must be name=value
error: line 3: syntax error: expected ')', found '=' (use == to compare)

=== diff
diff(x^3+2*x, x)
diff((x+1)^3, x)
diff(x^2*y^3, y)
diff(x^5, x, 3)
diff(x^5, x, 0)
diff(1/x, x)
diff(x^(1/2), x)
diff(x^n, x)
diff(5, x)
---
3*x^2+2
3*(x+1)^2
3*x^2*y^2
60*x^2
x^5
-1/x^2
1/(2*x^(1/2))
n*x^(n-1)
0

=== diff treats its variable as free even if assigned
x = 3;
diff(x^2, x)
x
---
2*x
3

=== diff errors
diff(sin(x)^x, y)
diff(x^2, 3)
---
0
error: line 2: diff: argument 2 must be a variable name

=== deg and coeff
deg(expand((x+1)^5), x)
deg((x+1)^5, x)
deg(x*y^2+x^3, y)
deg(0, x)
deg(7, x)
coeff((x+1)^5, x, 2)
coeff(3*x^2*y+x^2+5, x, 2)
coeff(3*x^2*y+x^2+5, x, 0)
coeff(x^2, x, 7)
---
5
5
2
-1
0
10
3*y+1
5
0

=== deg errors
deg(1/x, x)
deg(x^(1/2), x)
deg(x^y, x)
---
error: line 1: not a polynomial in x
error: line 2: not a polynomial in x
error: line 3: not a polynomial in x

=== polynomial division
quo(x^2-1, x-1, x)
rem(x^2-1, x-1, x)
quo(x^3+2, x-1, x)
rem(x^3+2, x-1, x)
quo(x^3-y^3, x-y, x)
rem(x^3-y^3, x-y, x)
quo(x^2+1, 2*x, x)
rem(x^2+1, 2*x, x)
quo(x, x^2, x)
rem(x, x^2, x)
---
x+1
0
x^2+x+1
3
x^2+x*y+y^2
0
x/2
1
0
x

=== polynomial division by zero
quo(x, 0, x)
---
error: line 1: division by zero polynomial

=== unknown functions and misuse
f(3)
expand(x, y)
1 + unset(x)
---
error: line 1: unknown function 'f'
error: line 2: expand: expected 1 argument
error: line 3: unset() can only be used as a statement on its own

=== built-in calls in formulas are re-evaluated, in snapshots not
p := expand((x+a)^2); s = expand((x+a)^2);
p
a = 1;
p
s
eval(s)
---
a^2+2*a*x+x^2
x^2+2*x+1
a^2+2*a*x+x^2
x^2+2*x+1
