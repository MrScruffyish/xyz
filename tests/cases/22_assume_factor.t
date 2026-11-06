# Assumptions, more factoring, half angles.
=== without assumptions
sqrt(x^2)
sqrt(x^4)
abs(x)^2
abs(x)^3
ln(x*y)
sin(n*pi)
solve(x^2 = 4)
assumptions()
---
abs(x)
x^2
x^2
abs(x)^3
ln(x*y)
sin(n*pi)
x = -2, x = 2
no assumptions

=== positive and negative symbols
assume(x > 0)
sqrt(x^2)
abs(x)
abs(-3x)
(x^3)^(1/3)
ln(2x)
ln(x^3)
ln(x/2)
ln(x*y)
assume(y < 0)
abs(y)
sqrt(y^2)
ln(x*y)
assume(0 <= z)
abs(z)
assumptions()
---
x
x
3*x
x
ln(2)+ln(x)
3*ln(x)
-ln(2)+ln(x)
ln(x*y)
-y
-y
ln(x*y)
z
x > 0
y < 0
z >= 0

=== solve with assumptions
assume(x > 0)
solve(x^2 = 4)
solve(x^3 = 8)
solve(x^2+1)
solve(x^2 - 2)
assume(t <= 0)
solve(t^2 = 9, t)
assume(n, integer)
solve(4n^2 = 9, n)
solve(n^2 = 9, n)
---
x = 2
x = 2
no solution
x = 2^(1/2)
t = -3
no solution
n = -3, n = 3

=== integer symbols
assume(n, integer)
sin(n*pi)
cos(n*pi)
cos(2*n*pi)
sin(x + 2*n*pi)
cos(x + n*pi)
(-1)^(4n)
(-1)^(3n)
sin(n*pi/2)
---
0
(-1)^n
1
sin(x)
(-1)^n*cos(x)
1
(-1)^n
sin(n*pi/2)

=== forget and errors
assume(x > 0)
forget(x)
sqrt(x^2)
assumptions()
assume(x)
assume(pi > 0)
assume(x == 0)
y = assume(x > 0)
---
abs(x)
no assumptions
error: line 5: assume: write assume(x > 0), assume(x >= 0), assume(x < 0), assume(x <= 0) or assume(n, integer)
error: line 6: assume: write assume(x > 0), assume(x >= 0), assume(x < 0), assume(x <= 0) or assume(n, integer)
error: line 7: assume: write assume(x > 0), assume(x >= 0), assume(x < 0), assume(x <= 0) or assume(n, integer)
error: line 8: assume() can only be used as a statement on its own

=== assumptions in integrals
integrate(sqrt(t^2), t)
integrate(abs(x), x, -1, 1)
assume(t > 0)
integrate(sqrt(t^2), t)
---
t*abs(t)/2
1
t^2/2

=== factor over roots in the coefficients
factor(x^2+2*sqrt(2)*x+2)
factor(x^2-2*sqrt(2)*x+1)
factor(3x^2+6*sqrt(3)*x+9)
factor(x^2-2)
---
(x+2^(1/2))^2
(x-2^(1/2)-1)*(x-2^(1/2)+1)
3*(x+3^(1/2))^2
x^2-2

=== factor: linear factors in several symbols
factor(x^2+(a+b)*x+a*b)
factor(x^2*y+x*y^2+x+y)
factor(x^2-y*z+x*z-x*y)
factor(a^2-b^2+a*c+b*c)
factor(x^2+x*y+x+y+x*z+y*z)
factor(x^2+y^2)
---
(a+x)*(b+x)
(x+y)*(x*y+1)
(x+z)*(x-y)
(a+b)*(a-b+c)
(x+y)*(x+z+1)
x^2+y^2

=== factor: 128-bit numbers
factor(1000000000012421*3000000000000781)
factor(2^125-1)
factor(10000000000000000000000000000000000043)
---
1000000000012421*3000000000000781
31*601*1801*269089806001*4710883168879506001
10000000000000000000000000000000000043

=== trigreduce: half angles
trigreduce(sin(2x)/(1+cos(2x)))
trigreduce((1-cos(2x))/sin(2x))
trigreduce(sin(x)/(1+cos(x)))
trigreduce(2*sin(2x)/(3+3cos(2x)))
trigreduce(tan(x)^2)
---
tan(x)
tan(x)
tan(x/2)
2*tan(x)/3
(-cos(2*x)+1)/(cos(2*x)+1)
