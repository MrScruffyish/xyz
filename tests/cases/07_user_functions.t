# f(x) := body  stores the body (evaluated at each call)
# f(x) =  body  evaluates the body once, at definition
=== ':=' function follows later changes, '=' function does not
c = 2;
f(x) := c*x
g(x) = c*x
c = 5;
f(1)
g(1)
---
f(x) := c*x
g(x) = 2*x
5
2

=== parameters are not affected by variables of the same name
x = 100;
f(x) := x^2+1
f(3)
f(y)
f(x)
---
f(x) := x^2+1
10
y^2+1
10001

=== several parameters and symbolic arguments
f(x, y) := x^2+y;
f(2, 3)
f(a+1, b)
expand(f(a+1, b))
---
7
(a+1)^2+b
a^2+2*a+b+1

=== the stored body keeps divisions, a call simplifies
f(x) := x/x
f(y)
f(0)
f(2)
---
f(x) := x/x
1
error: line 3: division by zero
1

=== only safe tidying when storing
f(x) := 2x+x
g(x) := x*x*x
h(x) := x^2/x
k(x) := 1/x-1/x
---
f(x) := 3*x
g(x) := x^3
h(x) := x^2/x
k(x) := 0/x

=== '=' function is simplified when defined
f(x) = x^2/x
f(0)
---
f(x) = x
0

=== a function defined from another one
f(x) := x/x;
g(a) := f(a)+1
g(y)
g(0)
f(x) := 2x;
g(5)
---
g(a) := f(a)+1
2
error: line 4: division by zero
11

=== difference quotient of a polynomial
f(x) := x^3+2x;
d := (f(x+h)-f(x))/h;
e = expand(d)
eval(e, h=0)
expand(diff(f(x), x))
expand((f(x+h)-f(x))/h) == 3x^2+3*x*h+h^2+2
---
e = h^2+3*h*x+3*x^2+2
3*x^2+2
3*x^2+2
1

=== difference quotient: eval cancels h first, subs divides by zero
f(x) := x^2;
d := (f(x+h)-f(x))/h;
eval(d, h=0)
subs(d, h=0)
---
2*x
error: line 4: division by zero

=== function errors
f(x) := x^2;
f(1, 2)
f
f := 3; f(2)
expand(x) := x
g(x, x) := 1
g(1) := 2
---
error: line 2: f: expected 1 argument
error: line 3: 'f' is a function; call it as f(...)
error: line 4: 'f' is a variable, not a function
error: line 5: cannot redefine the built-in function 'expand'
error: line 6: parameter 'x' appears twice
error: line 7: the parameters of g() must be names

=== recursion is stopped
f(x) := f(x)+1;
f(1)
---
error: line 2: too many nested function calls (recursive definition?)

=== functions without parameters
two() := 1+1
two()
---
two() := 2
2
