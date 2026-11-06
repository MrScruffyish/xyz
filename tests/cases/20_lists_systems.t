# Lists, seq/sum, and systems of equations.
=== lists and indexing
l = [1, 2, x]
l[3]
length(l)
[]
[[1, 2], [3, 4]][2][1]
l[4]
x[1]
l[0]
---
l = [1, 2, x]
x
3
[]
3
error: line 6: index 4 is not in 1..3
error: line 7: only lists can be indexed: x[...]
error: line 8: index 0 is not in 1..3

=== arithmetic element by element
l = [1, 2, x]
l + 1
2*l
l^2
l/2
l + [10, 20, 30]
[1, 2] + [1, 2, 3]
[1, 2] == [1, 2]
[1, 2] == [2, 1]
[1, 2] < [3, 4]
---
l = [1, 2, x]
[2, 3, x+1]
[2, 4, 2*x]
[1, 4, x^2]
[1/2, 1, x/2]
[11, 22, x+30]
error: line 7: lists of different lengths (2 and 3) cannot be combined
1
0
error: line 10: cannot compare [1, 2] < [3, 4]: both sides must be numbers

=== functions on lists
sin([0, pi/2])
diff([x^2, sin(x)], x)
num([1/3, sqrt(2)], 5)
expand([(x+1)^2, (x-1)^2])
factor([12, x^2-1])
eval([x, x^2], x=3)
deg([x^2, x^5+1], x)
log(2, [8, 4])
---
[0, 1]
[2*x, cos(x)]
[0.33333, 1.41421]
[x^2+2*x+1, x^2-2*x+1]
[2^2*3, (x-1)*(x+1)]
[3, 9]
[2, 5]
[3, 2]

=== seq and sum
seq(i^2, i = 1 to 5)
seq(k/2, k = 0 to 2 step 1/2)
seq([i, i^2], i = 1 to 3)
seq(i, i = 1 to 0)
sum(i, i = 1 to 100)
sum(1/2^k, k = 0 to 10)
sum(x^k, k = 0 to 3)
sum([1, 2, 3])
sum([[1, 2], [3, 4]])
sum(i, i = 1 to 0)
i
g(n) := sum(k, k = 1 to n)
g(10)
sum([1, 2], [3])
seq(i)
---
[1, 4, 9, 16, 25]
[0, 1/4, 1/2, 3/4, 1]
[[1, 1], [2, 4], [3, 9]]
[]
5050
2047/1024
x^3+x^2+x+1
6
[4, 6]
0
i
g(n) := sum(k, k=1 to n)
55
error: line 14: sum: the second argument must be a range, like i = 1 to 10
error: line 15: seq: expected 2 arguments

=== lists in variables and functions
f(t) = [t, t^2]
f(5)
m := [a, b]
a = 7
m
latex([1/2, x])
l = [1,
2]
l
---
f(t) = [t, t^2]
[5, 25]
m := [a, b]
a = 7
[7, b]
\left[\frac{1}{2}, x\right]
l = [1, 2]
[1, 2]

=== solve gives a list
s = solve(x^2-4)
s[2]
s + 1
length(solve(x^3-x))
solve(x^2-2)[1]^2
length(solve(1/x))
solve(x^5-x-1)[1]
---
s = x = -2, x = 2
2
[-1, 3]
3
2
0
error: line 7: solve: not all solutions were found, so there is no list of them; solve(e, x, k) gives solution number k

=== linear systems
solve([x+y=3, x-y=1], [x, y])
solve([x+y=3, x-y=1])
s = solve([2x+3y=5, 4x-y=3])
s[1]
solve([x+y+z=6, x-y=0, y-z=0])
solve([a*x+b*y=c, x-y=0], [x, y])
eqs = [x+y-3, x-y-1]
solve(eqs, [x, y])
latex(solve([x+y=3, x-y=1]))
---
x = 2, y = 1
x = 2, y = 1
s = x = 1, y = 1
1
x = 2, y = 2, z = 2
x = c/(a+b), y = c/(a+b)
eqs = [x+y-3, x-y-1]
x = 2, y = 1
x = 2,\quad y = 1

=== underdetermined and inconsistent systems
s = solve([x+y=1, 2x+2y=2], [x, y])
s
s[2]
solve([x+y=1, x+y=2], [x, y])
solve([x-x=0], [x])
---
s = x = -y+1
x = -y+1
y
no solution
every value is a solution

=== nonlinear systems by elimination
solve([x*y=1, x+y=2])
solve([x^2+y^2=25, x-y=1])
s = solve([x^2+y^2=1, y=x])
s[1]
solve([x^2=4, y^2=9])
solve([x*y=6, x^2*y=12])
solve([x*y=0, x+y=1])
solve([x*y=1, x*z=2, y*z=2])
---
x = 1, y = 1
x = -3, y = -4; x = 4, y = 3
s = x = 2^(1/2)/2, y = 2^(1/2)/2; x = -2^(1/2)/2, y = -2^(1/2)/2
[2^(1/2)/2, 2^(1/2)/2]
x = -2, y = -3; x = -2, y = 3; x = 2, y = -3; x = 2, y = 3
x = 2, y = 3
x = 1, y = 0; x = 0, y = 1
x = -1, y = -1, z = -2; x = 1, y = 1, z = 2

=== system errors
solve([x+y=1, x-y=1], [x, y], 1)
solve([x^5+y=1, y^5+x=3])
solve([x+y=1], [2])
---
error: line 1: solve: a system of equations has one solution; leave out k
error: line 2: solve: cannot solve x^25-5*x^20+10*x^15-10*x^10+5*x^5-x+2 = 0 in this system
error: line 3: solve: the symbols to solve for must be names, like [x, y]
