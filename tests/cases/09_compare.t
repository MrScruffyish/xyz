# a == b  is true exactly when expand(a - b) is 0.
=== numbers
1 == 1
1/2 == 0.5
2 < 3
3 <= 3
3 > 4
-1 >= -1
1 != 2
---
1
1
1
1
0
1
1

=== polynomials with free symbols
(x+1)^2 == x^2+2x+1
expand((x+1)^2-x^2-2x-1) == 0
(x+1)^2 == x^2+1
x*y == y*x
x/x == 1
(x^2-1)/(x-1) == x+1
---
1
1
0
1
1
1

=== logic
1 && 0
1 || 0
!0
!5
1 < 2 && 2 < 3
1 > 2 || 2 > 3
---
0
1
1
0
1
0

=== short-circuit
0 && (x > 1)
1 || (x > 1)
---
0
1

=== order comparisons need numbers
x < 1
x && 1
---
error: line 1: cannot compare x < 1: both sides must be numbers
error: line 2: condition is not a number: x

=== comparisons in formulas print readably
c := x < 3 && y == 2
d := !(a == b)
---
c := x < 3 && y == 2
d := !(a == b)
