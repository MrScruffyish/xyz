# Exact rational arithmetic (all numbers are exact, no rounding).
=== integer arithmetic
1+2*3
(1+2)*3
10-3-2
2^10
2^3^2
-2^2
(-2)^2
(-2)^3
---
7
9
5
1024
512
-4
4
-8

=== rationals are exact
1/3
1/3+1/6
2/4
-6/4
1/3*3
(2/3)^-2
(1/2)^10
---
1/3
1/2
1/2
-3/2
1
9/4
1/1024

=== decimals are exact rationals
0.1+0.2
0.1+0.2-0.3
1.5
0.125
00012.5000
.5+.5
5.
3.14159
---
3/10
0
3/2
1/8
25/2
1
5
314159/100000

=== big integers
2^100
2^64-1
123456789012345678901234567890*987654321098765432109876543210
(10^30+1)*(10^30-1)
-(2^70)
---
1267650600228229401496703205376
18446744073709551615
121932631137021795226185032733622923332237463801111263526900
999999999999999999999999999999999999999999999999999999999999
-1180591620717411303424

=== big rationals reduce
2^100/2^98
(3^40)/(3^38*9)
12345678901234567890/9876543210
---
4
1
137174210013717421/109739369

=== zero and one powers
0^0
5^0
0^5
1^1000000
(-1)^1001
(1/2)^0
---
1
1
0
1
-1
1

=== unary signs
--3
-+-3
+5
-(-(-1))
2*-3
2^-1
---
3
3
5
-1
-6
1/2

=== division by zero
1/0
0^-1
(1-1)^-2
x/(2-2)
---
error: line 1: division by zero
error: line 2: division by zero
error: line 3: division by zero
error: line 4: division by zero

=== non-integer powers of numbers stay exact
2^(1/2)
2^(1/2)*2^(1/2)
4^(1/2)
0^(1/2)
1^(1/3)
8^(1/3)
---
2^(1/2)
2
2
0
1
8^(1/3)

=== num gives decimal approximations (truncated like bc)
num(1/3)
num(2/3, 5)
num(-2/3)
num(1/7, 50)
num(5)
num(1/8, 0)
num(1/8, 2)
num(22/7, 10)
---
0.33333333333333333333
0.66666
-0.66666666666666666666
0.14285714285714285714285714285714285714285714285714
5
0
0.12
3.1428571428

=== num errors
num(x)
num(1/3)+1
num(1/3, -1)
num(1/3, 1/2)
---
error: line 1: num: not a number: x
error: line 2: a num() result cannot be used in further calculations
error: line 3: num: argument 2 must be an integer in [0, 1000000]
error: line 4: num: argument 2 must be an integer in [0, 1000000]
