# Automatic simplification: like terms and powers are combined, but
# products of sums are not multiplied out (use expand()).
=== like terms
2*x+3*x
2*x+3*x-x
x-x
x+y-x
3*x*y-2*y*x
---
5*x
4*x
0
y
x*y

=== like factors
x*x
x*x*x
x*x*y/x
x^2*x^3
x^a*x^b
x^a/x^a
2^a*2^b
---
x^2
x^3
x*y
x^5
x^a*x^b
1
2^(a+b)

=== powers of powers and products
(x^2)^3
(x*y)^3
(2*x)^2
(2*x)^-2
(x^(1/2))^2
(x^2)^(1/2)
x^(1/2)*x^(1/2)
x^(1/2)/x^(1/2)
x^(-1/2)
x^(3/2)/x^(1/2)
x^0
x^1
---
x^6
x^3*y^3
4*x^2
1/(4*x^2)
(x^(1/2))^2
(x^2)^(1/2)
(x^(1/2))^2
1
1/x^(1/2)
x^(3/2)/x^(1/2)
1
x

=== sums are kept factored
(x+2)^2
(x+2)*(x+2)
(x+1)*(x-1)
(x+1)^2-(x+1)^2
(x+1)*(x+1)/(x+1)
---
(x+2)^2
(x+2)^2
(x-1)*(x+1)
0
x+1

=== numbers distribute over a single sum
2*(x+1)
-(x-y)
x-(x+1)
(x+1)/2
---
2*x+2
-x+y
-1
x/2+1/2

=== term order: by degree, then alphabetically
1+x+x^2
y+x
y^2+x*y+x^2
z+y^3+x^2+1
3+x*y*z+x^3
---
x^2+x+1
x+y
x^2+x*y+y^2
y^3+x^2+z+1
x^3+x*y*z+3

=== printing quotients
1/x
x^-2*y
2/(3*x*y^2)
1/(x+1)
(x+1)/(x-1)
3/(4*(x+y)^2)
-x/2
x/2+y/3
1.5*x
---
1/x
y/x^2
2/(3*x*y^2)
1/(x+1)
(x+1)/(x-1)
3/(4*(x+y)^2)
-x/2
x/2+y/3
3*x/2

=== printing powers
x^(1/2)
x^(-1/2)
(1/2)^x
(-1)^x
2^-x
x^y^z
(x^y)^z
---
x^(1/2)
1/x^(1/2)
(1/2)^x
(-1)^x
2^(-x)
x^(y^z)
(x^y)^z
