# Elementary functions: exact values, symmetries, rules; anything else stays.
=== exact values of sin, cos, tan
disp(sin(0), sin(pi/6), sin(pi/4), sin(pi/3), sin(pi/2), sin(pi), sin(7pi/6), sin(-pi/4))
disp(cos(0), cos(pi/3), cos(3pi/4), cos(pi), cos(5pi/3), cos(pi/2))
disp(tan(0), tan(pi/4), tan(pi/6), tan(-pi/3), tan(pi))
tan(pi/2)
---
0	1/2	2^(1/2)/2	3^(1/2)/2	1	0	-1/2	-2^(1/2)/2
1	1/2	-2^(1/2)/2	-1	1/2	0
0	1	3^(1/2)/3	-3^(1/2)	0
error: line 4: tan(pi/2) is undefined

=== other values stay
disp(sin(1), cos(x), tan(pi/5), sin(pi/12))
---
sin(1)	cos(x)	tan(pi/5)	sin(pi/12)

=== symmetries and periods
disp(sin(-x), cos(-x), tan(-x), sin(x+pi), cos(x+2pi), sin(pi-x), tan(x+pi), cos(x-pi))
---
-sin(x)	cos(x)	-tan(x)	-sin(x)	cos(x)	sin(x)	tan(x)	-cos(x)

=== inverse functions
disp(asin(1/2), asin(-1), asin(sqrt(2)/2), acos(1/2), acos(-1/2), acos(0), acos(1))
disp(atan(1), atan(-sqrt(3)), atan(sqrt(3)/3), atan(0), asin(-x), acos(-x), atan(-x))
disp(sin(asin(x)), cos(acos(x)), tan(atan(x)), asin(sin(x)))
---
pi/6	-pi/2	pi/4	pi/3	2*pi/3	pi/2	0
pi/4	-pi/3	pi/6	0	-asin(x)	pi-acos(x)	-atan(x)
x	x	x	asin(sin(x))

=== exp and ln
disp(exp(0), exp(ln(x)), ln(exp(x)), ln(1), exp(1))
disp(ln(-1), ln(-2), ln(1/3), ln(2/3))
disp(exp(pi*im), exp(pi*im/2), exp(pi*im/3), exp(2im))
ln(0)
---
1	x	x	0	exp(1)
im*pi	ln(2)+im*pi	-ln(3)	ln(2/3)
-1	im	1/2+3^(1/2)*im/2	exp(2*im)
error: line 4: ln(0) is undefined

=== log
disp(log(2, 8), log(2, 1/8), log(10, 1000), log(4, 2), log(8, 1/2), log(3, 1))
disp(log(x), log(2, x), log(2, 3))
log(1, 5)
log(-2, 5)
---
3	-3	3	1/2	-1/3	0
ln(x)	ln(x)/ln(2)	ln(3)/ln(2)
error: line 3: log: the base must be positive and not 1
error: line 4: log: the base must be positive and not 1

=== powers of exp
exp(x)*exp(y)
exp(x)^3
exp(x)*exp(-x)
exp(x)/exp(1)
---
exp(x+y)
exp(3*x)
1
exp(x-1)

=== sin(u)^2 + cos(u)^2
sin(x)^2 + cos(x)^2
3*sin(x)^2 + 3*cos(x)^2 + 1
y*sin(2x)^2 + y*cos(2x)^2
2*sin(x)^2 + cos(x)^2
sin(x)^2 + cos(y)^2
---
1
4
y
cos(x)^2+2*sin(x)^2
cos(y)^2+sin(x)^2

=== derivatives
disp(diff(sin(x), x), diff(cos(x^2), x), diff(tan(x), x), diff(exp(2x), x), diff(ln(x), x))
disp(diff(asin(x), x), diff(acos(x), x), diff(atan(x), x))
disp(diff(2^x, x), diff(x^x, x), diff(exp(sin(x)), x), diff(sin(x)*cos(x), x))
diff(Re(x^(1/2)), x)
---
cos(x)	-2*x*sin(x^2)	1/cos(x)^2	2*exp(2*x)	1/x
1/(-x^2+1)^(1/2)	-1/(-x^2+1)^(1/2)	1/(x^2+1)
2^x*ln(2)	x^x*(ln(x)+1)	cos(x)*exp(sin(x))	cos(x)^2-sin(x)^2
error: line 4: diff: cannot differentiate Re()

=== trigexpand
trigexpand(sin(2x))
trigexpand(cos(2x))
trigexpand(sin(x+y))
trigexpand(cos(x-y))
trigexpand(cos(3x))
trigexpand(tan(x+y))
trigexpand(sin(2x)+ln(x))
---
2*cos(x)*sin(x)
cos(x)^2-sin(x)^2
cos(x)*sin(y)+cos(y)*sin(x)
cos(x)*cos(y)+sin(x)*sin(y)
cos(x)^3-3*cos(x)*sin(x)^2
(cos(x)*sin(y)+cos(y)*sin(x))/(cos(x)*cos(y)-sin(x)*sin(y))
2*cos(x)*sin(x)+ln(x)

=== real and imaginary parts
Re(exp(2im))
Im(exp(2im))
Re(exp(x+im*y))
abs(exp(im*x))
Re(sin(x))
Re(sin(im*x))
Re(ln(2))
Re(ln(x))
---
cos(2)
sin(2)
cos(y)*exp(x)
1
sin(x)
Re(sin(im*x))
ln(2)
Re(ln(x))

=== pi is a constant
pi = 3
f(pi) := 1
unset(pi)
pi
---
error: line 1: 'pi' is a constant and cannot be assigned
error: line 2: 'pi' is a constant and cannot be assigned
error: line 3: unset: 'pi' is a constant, not a variable
pi

=== LaTeX
latex(sin(x)^2 + exp(x) + ln(x) + atan(x) + acos(y) + pi)
---
\sin\left(x\right)^{2}+\pi+\arccos\left(y\right)+\arctan\left(x\right)+e^{x}+\ln\left(x\right)
