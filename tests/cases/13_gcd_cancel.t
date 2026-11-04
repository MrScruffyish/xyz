=== gcd and lcm of numbers
gcd(12, 18)
gcd(12, 18, 8)
gcd(-12, 18)
gcd(0, 5)
gcd(0, 0)
lcm(4, 6)
lcm(4, 6, 10)
lcm(0, 3)
---
6
2
6
5
0
12
60
0

=== gcd and lcm of fractions
gcd(1/2, 3/4)
lcm(1/2, 3/4)
gcd(6/5, 4/15)
---
1/4
3/2
2/15

=== gcd of polynomials
gcd(x^2-1, x-1)
gcd(x^2-1, x^2+2x+1)
gcd(x^2+1, x+1)
gcd(2x+4, 6x+12)
gcd(x/2+1, x+2)
gcd(-x+1, x^2-1)
gcd(x^3-x, x^2-x, x^2+x)
---
x-1
x+1
1
x+2
x+2
x-1
x

=== gcd of multivariate polynomials
gcd(x^2*y-x*y^2, x^2-y^2)
gcd(x^3-y^3, x^2-y^2)
gcd(x*y, x*z)
gcd(expand((x+y)^2*(x-z)), expand((x+y)*(x-z)^3))
---
x-y
x-y
x
x^2+x*y-x*z-y*z

=== lcm of polynomials
lcm(x^2-1, x+1)
lcm(x, y)
lcm(x+1, x-1)
---
x^2-1
x*y
x^2-1

=== gcd needs polynomials
gcd(1/x, x)
gcd(x^(1/2), x)
---
error: line 1: gcd: not a polynomial: 1/x
error: line 2: gcd: not a polynomial: x^(1/2)

=== cancel
cancel((x^2-1)/(x-1))
cancel((x^2-y^2)/(x-y))
cancel(x/x)
cancel((x+1)^2/(x^2-1))
cancel((2x+2)/(4x+4))
cancel(0/(x-1))
cancel(x^2+1)
---
x+1
x+y
1
(x+1)/(x-1)
1/2
0
x^2+1

=== cancel brings sums to one fraction
cancel(1/x+1/y)
cancel(1/(x-1)-1/(x+1))
cancel(x+1/x)
---
(x+y)/(x*y)
2/(x^2-1)
(x^2+1)/x

=== cancel treats roots like symbols
cancel(x^(1/2)/x^(1/2))
cancel((x^(1/2)+1)^2/((x^(1/2)+1)*y))
---
1
(x^(1/2)+1)/y

=== cancel and division by zero
cancel(1/(x-x))
---
error: line 1: division by zero

=== == is exact for quotients of polynomials
(x^2-1)/(x-1) == x+1
1/x+1/y == (x+y)/(x*y)
1/(x-1)-1/(x+1) == 2/(x^2-1)
1/x == 1/y
---
1
1
1
0

=== eval cancels before inserting, subs does not
eval((x^2-1)/(x-1), x=1)
subs((x^2-1)/(x-1), x=1)
eval(x+1/x, x=0)
---
2
error: line 2: division by zero
error: line 3: division by zero

=== stored := formulas are not cancelled
f(x) := (x^2-1)/(x-1)
f(2)
f(1)
cancel(f(x))
---
f(x) := (x^2-1)/(x-1)
3
error: line 3: division by zero
x+1
