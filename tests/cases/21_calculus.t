# Hyperbolic functions, integrate(), taylor() and limit().
=== hyperbolic functions
disp(sinh(0), cosh(0), tanh(0), asinh(0), acosh(1), atanh(0))
disp(sinh(-x), cosh(-x), tanh(-x), asinh(-x), atanh(-x))
disp(sinh(asinh(x)), cosh(acosh(x)), asinh(sinh(x)))
cosh(x)^2 - sinh(x)^2
3*cosh(2x)^2 - 3*sinh(2x)^2 + 1
disp(diff(sinh(x),x), diff(cosh(x),x), diff(tanh(x),x))
disp(diff(asinh(x),x), diff(acosh(x),x), diff(atanh(x),x))
num([sinh(1), cosh(1), tanh(1), asinh(1), acosh(2), atanh(1/2)], 15)
num(sinh(1+im), 10)
disp(Re(sin(im*x)), Im(sin(im*x)), Re(cosh(x+im*y)))
latex(asinh(x)+cosh(x))
atanh(1)
---
0	1	0	0	0	0
-sinh(x)	cosh(x)	-tanh(x)	-asinh(x)	-atanh(x)
x	x	x
1
4
cosh(x)	sinh(x)	1/cosh(x)^2
1/(x^2+1)^(1/2)	1/(x^2-1)^(1/2)	1/(-x^2+1)
[1.175201193643801, 1.543080634815244, 0.761594155955765, 0.881373587019543, 1.316957896924817, 0.549306144334055]
0.6349639148+1.2984575814*im
0	sinh(x)	cos(y)*cosh(x)
\operatorname{arsinh}\left(x\right)+\cosh\left(x\right)
error: line 12: atanh(1) is undefined

=== integrate: table and linear arguments
integrate(x^2, x)
integrate(3x^2+2x+1, x)
integrate(1/x, x)
integrate(x^(-3), x)
integrate(sqrt(x), x)
integrate((2x+1)^5, x)
integrate(exp(3x), x)
integrate(2^x, x)
integrate(sin(2x), x)
integrate(cos(x), x)
integrate(tan(x), x)
integrate(ln(x), x)
integrate(atan(x), x)
---
x^3/3
x^3+x^2+x
ln(x)
-1/(2*x^2)
2*x^(3/2)/3
(2*x+1)^6/12
exp(3*x)/3
2^x/ln(2)
-cos(2*x)/2
sin(x)
-ln(cos(x))
x*ln(x)-x
x*atan(x)-ln(x^2+1)/2

=== integrate: rational functions
integrate(1/(x^2+1), x)
integrate(1/(x^2-1), x)
integrate(1/(x^2+2x+5), x)
integrate((x^3+1)/(x^2-1), x)
integrate(1/(x*(x+1)^2), x)
integrate(1/(x^2+1)^2, x)
integrate(x/(x^2+1), x)
integrate(1/(x^2-2), x)
integrate(1/(x^3+1), x)
---
atan(x)
ln(x-1)/2-ln(x+1)/2
atan(x/2+1/2)/2
x^2/2+ln(x-1)
ln(x)-ln(x+1)+1/(x+1)
atan(x)/2+x/(2*(x^2+1))
ln(x^2+1)/2
2^(1/2)*ln((x-2^(1/2))/(x+2^(1/2)))/4
3^(1/2)*atan(2*3^(1/2)*(x-1/2)/3)/3+ln(x+1)/3-ln(x^2-x+1)/6

=== integrate: roots, by parts, substitution
integrate(1/sqrt(1-x^2), x)
integrate(sqrt(1-x^2), x)
integrate(1/sqrt(x^2+1), x)
integrate(x*exp(x), x)
integrate(x^2*sin(x), x)
integrate(x*ln(x), x)
integrate(x^2*atan(x), x)
integrate(exp(x)*sin(x), x)
integrate(exp(2x)*cos(3x), x)
integrate(x*exp(x^2), x)
integrate(sin(x)^2*cos(x), x)
integrate(ln(x)/x, x)
integrate(cos(x)/sin(x), x)
---
asin(x)
x*(-x^2+1)^(1/2)/2+asin(x)/2
asinh(x)
x*exp(x)-exp(x)
-x^2*cos(x)+2*x*sin(x)+2*cos(x)
x^2*ln(x)/2-x^2/4
x^3*atan(x)/3-x^2/6+ln(x^2+1)/6
(-cos(x)+sin(x))*exp(x)/2
(2*cos(3*x)+3*sin(3*x))*exp(2*x)/13
exp(x^2)/2
sin(x)^3/3
ln(x)^2/2
-ln(1/sin(x))

=== integrate: trigonometric, hyperbolic
integrate(sin(x)^2, x)
integrate(sin(x)*cos(x), x)
integrate(cos(x)^3, x)
integrate(sinh(x), x)
integrate(1/cos(x)^2, x)
integrate(tan(x)^2, x)
integrate(sin(x)^5, x)
---
x/2-sin(2*x)/4
-cos(x)^2/2
3*sin(x)/4+sin(3*x)/12
cosh(x)
tan(x)
-x+tan(x)
-5*cos(x)/8+5*cos(3*x)/48-cos(5*x)/80

=== integrate: not found, symbols
integrate(exp(x^2), x)
integrate(x^2 + exp(x^2), x)
integrate(a*x+b, x)
integrate(1/(x^2+a), x)
integrate(1/(x^3-2), x)
integrate(x*sqrt(x+1), x)
integrate(1/(1+exp(x)), x)
---
integrate(exp(x^2), x)
x^3/3+integrate(exp(x^2), x)
a*x^2/2+b*x
atan(x/a^(1/2))/a^(1/2)
integrate(1/(x^3-2), x)
-2*(x+1)^(3/2)/3+2*(x+1)^(5/2)/5
x-ln(exp(x)+1)

=== definite integrals
integrate(x^2, x, 0, 1)
integrate(sin(x), x, 0, pi)
integrate(1/(x^2+1), x, 0, 1)
integrate(exp(x^2), x, 0, 1)
latex(integrate(exp(x^2), x, 0, 1))
diff(integrate(exp(x^2), x), x)
integrate(exp(-x), x, 0, inf)
integrate(1/x^2, x, 1, inf)
integrate(ln(x), x, 0, 1)
integrate(1/(x^2+1), x, -inf, inf)
integrate(1/x, x, 1, inf)
integrate([x, x^2], x, 0, 1)
integrate(x, x, 1)
---
1/3
2
pi/4
integrate(exp(x^2), x, 0, 1)
\int_{0}^{1} e^{x^{2}} \, dx
exp(x^2)
1
1
-1
pi
inf
[1/2, 1/3]
error: line 13: integrate: give both bounds, integrate(f, x, a, b)

=== taylor
taylor(sin(x), x, 7)
taylor(exp(x), x = 0, 4)
taylor(ln(x), x = 1, 4)
taylor(1/(1-x), x, 5)
taylor(sqrt(1+x), x, 3)
taylor(cos(x), x = pi, 4)
taylor(sin(x)/x, x, 4)
taylor(ln(x), x = 0, 3)
taylor(sin(x), x)
taylor([exp(x), cos(x)], x, 3)
taylor(1/x, x = 2, 3)
---
-x^7/5040+x^5/120-x^3/6+x
x^4/24+x^3/6+x^2/2+x+1
-(x-1)^4/4+(x-1)^3/3-(x-1)^2/2+x-1
x^5+x^4+x^3+x^2+x+1
x^3/16-x^2/8+x/2+1
-(-pi+x)^4/24+(-pi+x)^2/2-1
x^4/120-x^2/6+1
error: line 8: taylor: ln(x) has no Taylor series at x = 0
x^5/120-x^3/6+x
[x^3/6+x^2/2+x+1, -x^2/2+1]
-(x-2)^3/16+(x-2)^2/8-x/4+1

=== limits
limit(sin(x)/x, x = 0)
limit((x^2-1)/(x-1), x = 1)
limit((1-cos(x))/x^2, x = 0)
limit(x*ln(x), x = 0, right)
limit(1/x, x = 0)
limit(1/x, x = 0, right)
limit(1/x, x = 0, left)
limit(1/x^2, x = 0)
limit((1+1/x)^x, x = inf)
limit((3x^2+1)/(x^2-5), x = inf)
limit(x/exp(x), x = inf)
limit(exp(x)/x^3, x = inf)
limit(atan(x), x = -inf)
limit(sqrt(x^2+x)-x, x = inf)
limit(x^x, x = 0, right)
limit(sin(x), x = inf)
limit(ln(x), x = 0, right)
limit((exp(x)-1-x)/x^2, x = 0)
limit(tan(x), x = pi/2, left)
limit(x^2, x = a)
limit(x^2*exp(-x), x = inf)
limit(ln(x)/x, x = inf)
limit(x*sin(1/x), x = inf)
limit((x+1)/(2x-3), x = -inf)
limit(x^3-x, x = -inf)
limit(sqrt(x^2+1)/x, x = -inf)
limit(exp(-1/x^2), x = 0)
limit((1+2/x)^(3x), x = inf)
limit(sin(3x)/sin(5x), x = 0)
limit(x^(1/x), x = inf)
---
1
2
1/2
0
error: line 5: limit: does not exist (-inf on one side, inf on the other); try limit(f, x = a, left) or right
inf
-inf
inf
exp(1)
3
0
inf
-pi/2
1/2
1
error: line 16: limit: sin(inf) does not exist
-inf
1/2
inf
a^2
0
0
1
1/2
-inf
-1
0
exp(6)
3/5
1

=== limit errors and inf
inf = 3
limit(1/x, x = 0, up)
limit(x, 0)
limit(1/x, x = 0, right) + 1
---
error: line 1: 'inf' is a constant and cannot be assigned
error: line 2: limit: the direction is left or right
error: line 3: limit: write the point as x = a, like limit(f, x = 0)
inf+1
=== poles between the bounds
integrate(1/x^2, x, -1, 1)
integrate(1/x^2, x, 1, 2)
integrate(1/(x^2-4), x, 0, 3)
integrate(1/x, x, 0, 1)
---
error: line 1: integrate: 1/x^2 has a pole at x = 0, between the bounds
1/2
error: line 3: integrate: 1/(x^2-4) has a pole at x = 2, between the bounds
inf
