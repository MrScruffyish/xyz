# expand() multiplies out products and integer powers of sums.
=== binomials
expand((x+2)^2)
expand((x-1)*(x+1))
expand((x+y)^3)
expand((x-y)^2)
expand((2*x+3)^2)
---
x^2+4*x+4
x^2-1
x^3+3*x^2*y+3*x*y^2+y^3
x^2-2*x*y+y^2
4*x^2+12*x+9

=== trinomial
expand((a+b+c)^2)
---
a^2+2*a*b+2*a*c+b^2+2*b*c+c^2

=== nested products
expand((x+1)*(x+2)*(x+3))
expand(x*(x+1)^2)
expand((x+1)^2*(x-1)^2)
expand(((x+1)^2+1)^2)
---
x^3+6*x^2+11*x+6
x^3+2*x^2+x
x^4-2*x^2+1
x^4+4*x^3+8*x^2+8*x+4

=== cancellation
expand((x+1)*(x-1)-(x^2-1))
expand((x+y)^2-(x-y)^2)
(x+1)*(x-1)-(x^2-1)
---
0
4*x*y
-x^2+(x-1)*(x+1)+1

=== rational coefficients
expand((x/2+1/3)^2)
expand((0.5*x-1)^2)
---
x^2/4+x/3+1/9
x^2/4-x+1

=== quotients with sum denominators are kept
expand((x^2+1)/x)
expand((x+1)^3/(x+1))
expand((x+1)^-2*(x+1)^3)
expand(1/(x+1)^2)
---
x+1/x
x^2+2*x+1
x+1
1/(x+1)^2

=== expand of variables
p = (x+1)^2; q := (x-1)^2;
expand(p-q)
expand(p*q)
x = 2;
expand(p)
expand(q)
eval(expand(p))
---
4*x
x^4-2*x^2+1
x^2+2*x+1
1
9

=== larger power
expand((x+1)^10)
---
x^10+10*x^9+45*x^8+120*x^7+210*x^6+252*x^5+210*x^4+120*x^3+45*x^2+10*x+1

=== high power has correct central coefficient
coeff((x+1)^100, x, 50)
---
100891344545564193334812497256
