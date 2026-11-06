# Complex numbers: im is the imaginary unit; symbols are taken to be real.
=== powers of im
disp(im^2, im^3, im^4, im^5, im^-1, 1/im, im^0)
---
-1	-im	1	im	-im	-im	1

=== exact complex numbers are normalized to a+b*im
(3+im)*(5-3im)
1/(1+im)
(2+3im)/(4-im)
(1+im)^2
(1+im)^10
(1+im)^-2
2*(3+im)
im*(3+im)
3+2im-3
3+2im-2im
---
18-4*im
1/2-im/2
5/17+14*im/17
2*im
32*im
-im/2
6+2*im
-1+3*im
2*im
3

=== with symbols, products of sums are kept (as for real sums)
x*(3+im)
expand(x*(3+im))
expand((x+im)^2)
expand((x+im)*(x-im))
---
x*(3+im)
3*x+im*x
x^2+2*im*x-1
x^2+1

=== Re and Im
Re((3+im)*(5-3im))
Im((3+im)*(5-3im))
Re(x+im*y)
Im(x+im*y)
Re(1/(x+im))
Im(1/(x+im))
Re((x+im*y)^2)
Im((x+im*y)^2)
Re(5)
Im(5)
---
18
-4
x
y
x/(x^2+1)
-1/(x^2+1)
x^2-y^2
2*x*y
5
0

=== conj
conj(3+2im)
conj(x+im*y)
conj(1/(1+im))
conj(conj(z))
---
3-2*im
x-im*y
1/2+im/2
z

=== abs
abs(3+4im)
abs(1+im)
abs(-3)
abs(-3/4)
abs(im)
abs(-2im)
abs(x+im*y)
abs(x)
abs(-3x)
---
5
2^(1/2)
3
3/4
1
2
(x^2+y^2)^(1/2)
abs(x)
3*abs(x)

=== when the parts cannot be found, the function stays
Re(x^(1/2))
Im(x^(1/2))
abs(x^(1/2))
conj(x^(1/2))
Re(x^(1/2)) + 1
eval(Re(x^(1/2)), x=4)
eval(Re(x^(1/2)), x=-4)
---
Re(x^(1/2))
Im(x^(1/2))
abs(x^(1/2))
conj(x^(1/2))
Re(x^(1/2))+1
2
0

=== results of Re, Im and abs are real
Re(Re(x^(1/2)) + im)
Im(Re(x^(1/2)) + im)
Re(2^(1/2) + im)
---
Re(x^(1/2))
1
2^(1/2)

=== square roots
sqrt(16)
sqrt(8)
sqrt(18)
sqrt(1/4)
sqrt(2/9)
sqrt(-4)
sqrt(-2)
sqrt(x)
sqrt(2)*sqrt(2)
sqrt(2)^2
sqrt(8)^-1
2^(3/2)
---
4
2*2^(1/2)
3*2^(1/2)
1/2
2^(1/2)/3
2*im
2^(1/2)*im
x^(1/2)
2
2
2^(1/2)/4
2*2^(1/2)

=== decimals of complex numbers
num(1/3+2im/3, 5)
num(-im, 3)
num(-im/3, 3)
num(2-im, 3)
---
0.33333+0.66667*im
-im
-0.333*im
2-im

=== im cannot be assigned
im = 3
im := 3
f(im) := 1
for (im = 1 to 2) { }
eval(x, im=2)
unset(im)
---
error: line 1: 'im' is the imaginary unit and cannot be assigned
error: line 2: 'im' is the imaginary unit and cannot be assigned
error: line 3: 'im' is the imaginary unit and cannot be assigned
error: line 4: 'im' is the imaginary unit and cannot be assigned
error: line 5: 'im' is the imaginary unit and cannot be assigned
error: line 6: unset: 'im' is the imaginary unit, not a variable

=== comparisons
(1+im)^2 == 2im
1/(1+im) == (1-im)/2
3+im < 4
---
1
1
error: line 3: cannot compare 3+im < 4: both sides must be numbers

=== gcd and cancel over Q(i)
gcd(x-im, x-im)
cancel((x^2+1)/(x-im))
cancel(1/(x-im) - 1/(x+im))
1/(x-im) - (x+im)/(x^2+1) == 0
---
x-im
x+im
2*im/(x^2+1)
1

=== gcd, cancel and factor over Q(i)
gcd(x^2+1, x-im)
gcd(x^2+1, x^2+2*im*x-1)
gcd(x^2-1, x-1)
cancel((x^2+2*im*x-1)/(x^2+1))
factor(x^2+1)
factor(x^2+1, im)
factor(x^4+1, im)
factor(x^2+2*im*x-1)
factor(x^4-1, im)
factor(2x^2+2, im)
factor(x^3-im)
factor((x^2+1)/(x-im))
factor(1/(x^2+1), im)
factor(x^2+x+1, im)
factor(x^4+4, im)
factor(x^2+y^2, im)
factor(x^3+x*y^2, im)
factor([x^2+1, x^4+1], im)
factor(x^2+1, 2)
---
x-im
x+im
x-1
(x+im)/(x-im)
x^2+1
(x+im)*(x-im)
(x^2+im)*(x^2-im)
(x+im)^2
(x-1)*(x+1)*(x+im)*(x-im)
2*(x+im)*(x-im)
(x+im)*(x^2-im*x-1)
x+im
1/((x+im)*(x-im))
x^2+x+1
(x-1+im)*(x-1-im)*(x+1+im)*(x+1-im)
(x-im*y)*(x+im*y)
x*(x-im*y)*(x+im*y)
[(x+im)*(x-im), (x^2+im)*(x^2-im)]
error: line 19: factor: the second argument can only be im, to factor over the complex rationals
