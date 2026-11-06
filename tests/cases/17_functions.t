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
(tan(x)+tan(y))/(-tan(x)*tan(y)+1)
2*cos(x)*sin(x)+ln(x)

=== trigreduce
trigreduce(2*sin(x)*cos(x))
trigreduce(cos(x)^2-sin(x)^2)
trigreduce(cos(x)^2)
trigreduce(sin(x)^3)
trigreduce(cos(x)^4)
trigreduce(cos(x)*cos(y)-sin(x)*sin(y))
trigreduce(sin(x)*cos(y))
trigreduce(sin(x)*sin(2x)*sin(3x))
trigreduce((sin(x)+cos(x))^2)
trigreduce(x*sin(x)*cos(x)+1)
---
sin(2*x)
cos(2*x)
cos(2*x)/2+1/2
3*sin(x)/4-sin(3*x)/4
cos(2*x)/2+cos(4*x)/8+3/8
cos(x+y)
sin(x+y)/2+sin(x-y)/2
sin(2*x)/4+sin(4*x)/4-sin(6*x)/4
sin(2*x)+1
x*sin(2*x)/2+1

=== trigreduce undoes trigexpand
disp(trigreduce(trigexpand(cos(3x))), trigreduce(trigexpand(sin(5x))), trigreduce(trigexpand(cos(x-y))))
---
cos(3*x)	sin(5*x)	cos(x-y)

=== trigreduce leaves the rest
disp(trigreduce((x+1)^2), trigreduce(exp(sin(x)*cos(x))), trigreduce(tan(x)+1))
trigreduce(sin(x)^2/cos(x))
trigreduce(sin(x)^2000)
---
(x+1)^2	exp(sin(2*x)/2)	tan(x)+1
(-cos(2*x)/2+1/2)/cos(x)
error: line 3: trigreduce: the powers of sin and cos are too large

=== trigexpand of tan
disp(trigexpand(tan(x-y)), trigexpand(tan(2x)), trigexpand(tan(x+pi/4)), trigexpand(tan(x+pi/2)))
trigexpand(tan(3x))
---
(tan(x)-tan(y))/(tan(x)*tan(y)+1)	2*tan(x)/(-tan(x)^2+1)	(tan(x)+1)/(-tan(x)+1)	-1/tan(x)
(-tan(x)^3+3*tan(x))/(-3*tan(x)^2+1)

=== trigreduce with tan
disp(trigreduce((tan(x)+tan(y))/(1-tan(x)*tan(y))), trigreduce(2*tan(x)/(1-tan(x)^2)), trigreduce(trigexpand(tan(3x))))
disp(trigreduce(sin(x)/cos(x)), trigreduce(cos(x)/sin(x)), trigreduce(tan(x)*cos(x)), trigreduce(tan(x)*sin(x)))
disp(trigreduce(tan(x)^2), trigreduce(tan(x)^2+1), trigreduce(tan(x)/(1+tan(x)^2)))
---
tan(x+y)	tan(2*x)	tan(3*x)
tan(x)	1/tan(x)	sin(x)	(-cos(2*x)/2+1/2)/cos(x)
(-cos(2*x)+1)/(cos(2*x)+1)	2/(cos(2*x)+1)	sin(2*x)/2

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
