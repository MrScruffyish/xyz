# Variables: '=' stores a snapshot (value now), ':=' stores a formula.
# A statement prints its result unless it ends with ';'.
=== assignments echo, ';' hides
a = 4
b = a*a+3
a = 1;
b
---
a = 4
b = 19
19

=== the last statement on a line is shown
a=1; b=a; b
---
1

=== every statement without ';' is shown
1; 2
3
---
2
3

=== '=' evaluates now
x = 1
a = x+1;
x = 10
a
---
x = 1
x = 10
2

=== ':=' follows later changes
x = 1;
a := x+1
x = 10;
a
---
a := x+1
11

=== snapshot with a free symbol does not change
p = (x+1)^2;
x = 3;
p
eval(p)
p + x
eval(p + x)
---
(x+1)^2
16
(x+1)^2+3
19

=== unset only removes the variable itself
p = (x+1)^2; q := (x+1)^2; x = 3;
q
unset(x)
q
p
---
16
(x+1)^2
(x+1)^2

=== the original linked example, written with ':='
a := x; b := a*a+3;
b
x = 1;
b
a = 5;
b
x
---
x^2+3
4
28
1

=== snapshot of a formula
a := x+3; x = 2;
b = a+1;
x = 100;
b
a
---
6
103

=== counting with '=' stores numbers
i = 0; i = i+1; i = i+1; i = i*10;
def(i)
---
i = 20

=== a = a*a squares the value
a = 3; a = a*a;
a
a = x; a = a*a;
a
---
9
x^2

=== a = a*a needs a value
c = c*c
c
---
error: line 1: 'c' has no value yet, so c = ... cannot use it
c

=== self reference in ':=' substitutes the old definition
a := x;
a := a+1
x = 10;
a
---
a := x+1
11

=== indirect self reference in ':='
a := x; b := a+1;
a := b*2
b
---
a := 2*x+2
2*x+3

=== recursive ':=' of an undefined variable
c := c+1
p := q; q := p
---
error: line 1: recursive definition: 'c' has no value yet
error: line 2: recursive definition: 'q' has no value yet

=== chains of formulas
a := b; b := c; c := d;
a
d = 7;
a
c = 2;
a
---
d
7
2

=== unset several and def errors
a = 1; b := 2;
unset(a, b)
a + b
def(a)
---
a+b
error: line 4: def: 'a' is not defined

=== def shows how a variable was stored
a := x+3; b = 4; f(t) := t^2; g(t) = 2t;
def(a)
def(b)
def(f)
def(g)
---
a := x+3
b = 4
f(t) := t^2
g(t) = 2*t

=== disp shows values, also mid-line and several at once
a := x+3; b := y+3; x = 2;
def(a); disp(a); def(b); disp(b);
disp(1, x, a)
---
a := x+3
5
b := y+3
y+3
1	2	5

=== names
alpha_1 = 3; Beta = 4;
alpha_1*Beta
_x = 2; _x^3
---
12
8

=== reserved words cannot be names
to = 1
step + 1
---
error: line 1: 'to' is a reserved word and cannot be used as a name
error: line 2: 'step' is a reserved word and cannot be used as a name

=== an incomplete statement
a = 1; b = 2;
c = a+
---
error: line 2: syntax error: expected a number, name or '(', found end of statement

=== newlines inside parentheses continue the statement
a = (1+
2+
3);
a
---
6

=== backslash newline continues the statement
1+\
2
---
3

=== comments
# a comment
1+1 # trailing comment
/* block
comment */ 2+2
---
2
4

=== quit stops processing
1
quit
2
---
1

=== halt is a synonym of quit
disp(1); halt; 2
---
1

=== variables named like built-in functions
expand = 3;
expand+1
expand(expand*x)
---
4
3*x

=== implicit multiplication after a number
x = 2;
3x
3x^2
2x+x
1.5x
---
6
12
6
3
