# factor() and solve().
=== factor of integers
factor(120)
factor(-360)
factor(12/35)
factor(97)
factor(1)
factor(0)
factor(2^64+1)
factor(2^128-1)
---
2^3*3*5
-2^3*3^2*5
2^2*3/(5*7)
97
1
0
274177*67280421310721
3*5*17*257*641*65537*274177*6700417*67280421310721

=== factor bounds
factor(10^301)
factor(3000000037894000009807031)
---
error: line 1: factor: numbers with more than 300 digits are not factored
error: line 2: factor: cannot factor 3000000037894000009807031 within the bound: it has no prime factor below 10^5, and Pollard's rho method (good for prime factors up to about 10^11) found none

=== a factored number computes as its value
f = factor(120)
f + 1
disp(f, f/8)
latex(factor(-12/35))
---
f = 2^3*3*5
121
2^3*3*5	15
-\frac{2^{2} \cdot 3}{5 \cdot 7}

=== factor of polynomials
factor(x^2-1)
factor(12x^2-12)
factor(-x^2+1)
factor(2x^2+x-1)
factor(x/2+1/3)
factor(x^3-x^2-x+1)
factor((x+1)^3*(x-2)^2)
factor(x^6-1)
factor(x^8-1)
---
(x-1)*(x+1)
12*(x-1)*(x+1)
-(x-1)*(x+1)
(x+1)*(2*x-1)
(3*x+2)/6
(x-1)^2*(x+1)
(x-2)^2*(x+1)^3
(x-1)*(x+1)*(x^2+x+1)*(x^2-x+1)
(x-1)*(x+1)*(x^2+1)*(x^4+1)

=== factor: Kronecker for factors of degree 2 and more
factor(x^4+4)
factor(x^4+x^2+1)
factor(x^5+x+1)
factor(x^2+1)
factor(x^4-10x^2-8x+5)
---
(x^2-2*x+2)*(x^2+2*x+2)
(x^2+x+1)*(x^2-x+1)
(x^2+x+1)*(x^3-x^2+1)
x^2+1
(x^2-2*x-5)*(x^2+2*x-1)

=== factor with several symbols
factor(a*x+a*b)
factor(x^2-y^2)
factor(x^3+y^3)
factor(x^2-2*x*y+y^2)
factor(x^2*y-y+x^2-1)
factor(a^2*x^2-a^2)
---
a*(b+x)
(x+y)*(x-y)
(x+y)*(x^2-x*y+y^2)
(x-y)^2
(x-1)*(x+1)*(y+1)
a^2*(x-1)*(x+1)

=== factor of fractions and other atoms
factor((x^2-1)/(x^2+2x+1))
factor(1/(x^2-1))
factor(sin(x)^2-1)
latex(factor(12x^2-12))
---
(x-1)/(x+1)
1/((x-1)*(x+1))
(sin(x)-1)*(sin(x)+1)
12 \left(x-1\right) \left(x+1\right)

=== solve: degree 1 and 2
solve(2x-4, x)
solve(x^2 = 4)
solve(x^2-2)
solve(x^2+x+1, x)
solve(x = 3)
solve(3x+1 = x-5)
---
x = 2
x = -2, x = 2
x = 2^(1/2), x = -2^(1/2)
x = -1/2+3^(1/2)*im/2, x = -1/2-3^(1/2)*im/2
x = 3
x = -3

=== solve with symbols as coefficients
solve(a*x+b, x)
solve(a*x^2+b*x+c, x)
solve(x^2-a^2, x)
solve(x^2+2*im*x-1, x)
---
x = -b/a
x = (-b+(-4*a*c+b^2)^(1/2))/(2*a), x = (-b-(-4*a*c+b^2)^(1/2))/(2*a)
x = -a, x = a
x = -im

=== solve: degree 3 and 4
solve(x^3-6x^2+11x-6)
solve(x^3-2)
solve(x^3-3x+1)
solve(x^3+3x-4)
solve(x^4-5x^2+6)
solve(x^4+4)
solve(x^4-10x^2-8x+5)
---
x = 1, x = 2, x = 3
x = 2^(1/3), x = 2^(1/3)*(-1/2+3^(1/2)*im/2), x = 2^(1/3)*(-1/2-3^(1/2)*im/2)
x = 2*cos(2*pi/9), x = 2*cos(4*pi/9), x = 2*cos(10*pi/9)
x = 1, x = -1/2+15^(1/2)*im/2, x = -1/2-15^(1/2)*im/2
x = 3^(1/2), x = -3^(1/2), x = 2^(1/2), x = -2^(1/2)
x = 1+im, x = 1-im, x = -1+im, x = -1-im
x = 6^(1/2)+1, x = -6^(1/2)+1, x = 2^(1/2)-1, x = -2^(1/2)-1

=== solve: higher degree and fractions
solve(x^6-1)
solve(x^5-x-1)
solve(x^7-x)
solve((x^2-1)/(x-1))
solve(1/x)
solve(x-x)
solve(3)
---
x = -1, x = 1, x = -1/2+3^(1/2)*im/2, x = -1/2-3^(1/2)*im/2, x = 1/2+3^(1/2)*im/2, x = 1/2-3^(1/2)*im/2
x^5-x-1 = 0
x = -1, x = 0, x = 1, x = -1/2+3^(1/2)*im/2, x = -1/2-3^(1/2)*im/2, x = 1/2+3^(1/2)*im/2, x = 1/2-3^(1/2)*im/2
x = -1
no solution
every value is a solution
no solution

=== solve: one solution as a value
r = solve(x^3-2, x, 1)
r^3
solve(x^2-4, x, 3)
s = solve(x^2-4)
s + 1
latex(solve(x^2-2))
---
r = 2^(1/3)
2
error: line 3: solve: there are only 2 solutions
s = x = -2, x = 2
error: line 5: solve() gives a list of solutions; solve(e, x, k) gives solution number k
x = \sqrt{2},\quad x = -\sqrt{2}

=== solve: errors
solve(sin(x))
solve(x+y)
x^2 = 4
sin(x = 2)
sin(x^2 = 4)
---
error: line 1: solve: can only solve polynomial equations in x (also with fractions of them)
error: line 2: solve: say which symbol to solve for, like solve(e, x)
error: line 3: can only assign to a name, or define a function such as f(x)
error: line 4: sin: 'name=value' arguments are not allowed here
error: line 5: an equation a = b can only be used in solve()
