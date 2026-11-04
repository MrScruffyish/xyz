# xyz

A small command-line computer algebra system in the spirit of `bc`. It
reads statements from standard input, files or `-e`, prints results, and
works well in pipes. All numbers are **exact**: arithmetic uses rationals
of arbitrary-precision integers, and the integer arithmetic is done by
**bcl**, the library form of [Gavin D. Howard's bc](https://github.com/gavinhoward/bc).

```console
$ echo 'a := x; b := a*a+3; b; eval(b, x=1)' | xyz
4
$ echo 'a := x; b := a*a+3; disp(b); eval(b, x=1)' | xyz
x^2+3
4
$ xyz -e 'expand((x+2)^2)'
x^2+4*x+4
$ xyz -e '1/3 + 1/6' -e 'num(1/7, 30)'
1/2
0.142857142857142857142857142857
$ xyz -e 'table(x, x^2, x = 1 to 3)'
1	1
2	4
3	9
```

## Building

You need a C++17 compiler, `make`, and a checkout of bc next to this
repository (or anywhere else, given as `BC_DIR`):

```sh
git clone https://github.com/gavinhoward/bc ../bc   # tested with 7.2.0 (1a8e2e65)
make                  # builds bcl out-of-tree in build/bcl, then ./xyz
make test             # golden, CLI and randomised tests
make test-asan        # the same under AddressSanitizer + UBSan
make BC_DIR=/path/to/bc
make READLINE=no      # build without line editing
```

If GNU readline (or libedit's compatible version, as on macOS) is
installed, `make` uses it for line editing at the terminal. On Debian and
Ubuntu it comes with `sudo apt install libreadline-dev`. Without it,
everything works the same, only without arrow keys and history.

The bc checkout is never modified. bcl is configured with
`configure.sh -a` (library only) in `build/bcl`.

## Usage

```
xyz [-h] [-v] [-e expr]... [file]...
```

* With no arguments, `xyz` reads standard input. If standard input is a
  terminal, each statement is run as soon as it is complete. Open
  parentheses or braces, or an `if (...)` still waiting for its `{`,
  continue onto the next line.
* At a terminal, the arrow keys edit the line and step through earlier
  lines (if built with readline). What you type is echoed on stderr, so
  `xyz > results.txt` still writes only results. Pipes and files are read
  without any line editing.
* `-e EXPR` runs `EXPR`. You can repeat it. When `-e` is used, standard
  input is only read if `-` is given as a file.
* Files and `-e` expressions all share the same variables, and run in order.
* Results go to stdout and errors go to stderr (`error: line N: ...`).
  Processing continues with the next statement after an error. The exit
  status is 0 if there were no errors, 1 if there were errors, and 2 for
  usage errors.

## Language

### Statements and output

Statements are separated by newlines or `;`. **A statement prints its
result unless it ends with `;`.** A newline, `}` or the end of the input
ends a statement and shows it.

```
a = 4              -> a = 4          (an assignment shows what was stored)
b = a*a+3;         -> (nothing)
b                  -> 19
a=1; b=a; b        -> 1              (only the last one has no ';')
```

| Statement                    | Effect                                              |
|------------------------------|-----------------------------------------------------|
| `expr`                       | evaluates `expr` and shows the result               |
| `name = expr`                | stores the value of `expr` now (a snapshot)         |
| `name := expr`               | stores `expr` itself (a formula)                    |
| `f(x, ...) = expr`           | defines a function, body evaluated now               |
| `f(x, ...) := expr`          | defines a function, body evaluated at each call      |
| `if`, `while`, `do`, `for`   | control flow, see below                             |
| `disp(e, ...)`               | prints values (always, also inside loops)           |
| `def(name)`                  | prints how `name` is stored                         |
| `unset(name, ...)`           | removes variables or functions                      |
| `table(...)`                 | prints a table of values, see below                 |
| `save("file")`               | writes the statements of this session to a file     |
| `transcript("file")`         | the same, with the output added as `#` comments     |
| `quit` or `halt`             | stops reading input                                 |

A statement continues onto the next line while parentheses are open, or
when the line ends with `\`. Comments are `# ... end of line` and
`/* ... */`, as in bc.

### Expressions

* Numbers: `42`, `3.25` (exact: 13/4), `.5`, and integers of any size.
* Names: letters, digits and `_`, not starting with a digit. Names are
  case sensitive. A name with no value is a symbol, e.g. `x`. The words
  `if else while do for to step quit halt` are reserved.
* A number directly followed by a name is a product: `2x` is `2*x` and
  `3x^2` is `3*x^2`. (`xy` is a single name.) Output always uses `*`.
* Operators, from lowest to highest precedence:

  | Operators                    | Meaning                                    |
  |------------------------------|--------------------------------------------|
  | `\|\|`                         | or                                         |
  | `&&`                         | and                                        |
  | `!`                          | not                                        |
  | `== != < <= > >=`            | comparisons (give 1 or 0)                  |
  | `+ -`                        | sum, difference                            |
  | `* /`                        | product, quotient                          |
  | unary `- +`                  | sign                                       |
  | `^`                          | power, right associative                   |

  `2^3^2` is 512, `-2^2` is -4 and `2^-1` is 1/2.
* Function calls: `name(arg, ...)`.

### Exact arithmetic

```
1/3+1/6          -> 1/2
0.1+0.2          -> 3/10
2^100            -> 1267650600228229401496703205376
(2/3)^-2         -> 9/4
6/4              -> 3/2      (fractions are reduced with Euclid's gcd)
2^(1/2)*2^(1/2)  -> 2        (non-integer powers stay symbolic)
1/0              -> error: division by zero
0^0              -> 1        (as in bc)
```

Results are never rounded. `num(e, digits)` gives a decimal
approximation. It truncates like bc and defaults to 20 digits. The result
of `num` is for display only and cannot be used in further calculations.

### Variables: `=` stores a value, `:=` stores a formula

| Form          | Meaning                                                        |
|---------------|----------------------------------------------------------------|
| `a = expr`    | take `expr` literally *now*: variables are replaced by their current values, the result is simplified and stored |
| `a := expr`   | take `expr` literally *for ever*: the expression is stored, and every use re-evaluates it with the values at that time |

```
x = 1;
a := x+1;        # a formula
b = a+1;         # evaluated now: stores 3
x = 10;
a                -> 11
b                -> 3
i = 0; i = i+1; i = i+1;     # i stores 2, not ((0+1)+1)
```

**`=` stores a snapshot. It never changes until that variable is
assigned again.** A variable without a value is a free symbol, and it
stays a plain symbol inside the snapshot. Giving it a value later does
not change the snapshot; `eval()` plugs in the current values when you
want them. Use `:=` when a result should follow its variables.

```
p = (x+1)^2;     # x has no value: p stores (x+1)^2
q := (x+1)^2;    # q stores the formula
x = 3;
p                -> (x+1)^2     (unchanged: assigning x has no side effects)
eval(p)          -> 16          (plug in the current values on request)
q                -> 16          (q follows x)
unset(x)         # removes only x's value; p and q are untouched
q                -> (x+1)^2
```

The one case to watch: after `x = 3`, the `x` inside `p` is still the
symbol, while a new `x` means 3:

```
p = (x+1)^2;  x = 3;
p + x            -> (x+1)^2+3
eval(p + x)      -> 19
```

Linked variables are written with `:=`:

```
a := x; b := a*a+3;
b                -> x^2+3
x = 1;
b                -> 4
a = 5;           # a is now the number 5
b                -> 28
```

**Self reference.** `a = a*a` squares the current value of `a`; it is an
error if `a` has no value. `a := a+1` substitutes the old definition of
`a` (`a := x; a := a+1` gives `x+1`, still linked to `x`); it is an error
if `a` has no definition.

`def(a)` shows how `a` is stored (`a = 3` or `a := x+1`), `disp(a)` shows
its value, and `unset(a)` turns `a` back into a free symbol.

### Functions

Functions follow the same rule as variables:

```
c = 2;
f(x) := c*x      # literally, for ever: c is looked up at every call
g(x) = c*x       # literally, now: c is replaced by 2, g stores 2*x
c = 5;
f(1)             -> 5
g(1)             -> 2
```

Parameters are local to the function body: a variable `x` outside does
not affect the parameter `x`. A call substitutes the arguments first and
then simplifies fully, so a division by zero is always caught. A `:=`
function is stored as written, with only tidying that cannot change where
it is defined (see "Simplification"):

```
f(x) := x/x
def(f)           -> f(x) := x/x
f(y)             -> 1          (y/y is simplified after substituting)
f(0)             -> error: division by zero

g(a) := f(a)+1   # refers to f by name: g follows later changes of f
g(0)             -> error: division by zero
h(x) = x^2/x     # evaluated once, now: stores x, so h(0) is 0
```

Functions can be called with symbolic arguments (`f(a+1)`) and can use
other functions. A function that calls itself without end is stopped
with an error. Built-in function names cannot be redefined.

The difference quotient of a polynomial:

```
f(x) := x^3+2x;
d := (f(x+h)-f(x))/h;
eval(expand(d), h=0)   -> 3*x^2+2      (= diff(f(x), x))
subs(expand(d), h=0)   -> error: division by zero
```

See the next section for the difference between `eval` and `subs`.

### `eval` and `subs`: plugging in values

Both put values into an expression without assigning anything. They
differ in *when* the values go in:

* **`subs(e, x=v)`**: **substitute first**, then evaluate and simplify.
  Every `x` in `e`, also inside the formulas and functions `e` uses, is
  replaced by `v` before anything is computed. A division by zero is never
  hidden.
* **`eval(e, x=v)`**: **evaluate and simplify first**, with `x` kept as a
  plain symbol (even if `x` has a value), then insert `v` and simplify
  again. Simplification may remove a division before the value goes in.

| Input | `eval` | `subs` |
|---|---|---|
| `(x^2+1, x=3)` | `10` | `10` |
| `(x/x, x=0)` | `1` | error: division by zero |
| `((x^2-1)/(x-1), x=1)` | `2` | error: division by zero |
| `(expand((f(x+h)-f(x))/h), h=0)`, `f(x) := x^3+2x` | `3*x^2+2` | error: division by zero |
| `(x^2, x=2)` after `x = 5` | `4` | `4` |
| `(b, a=2)` with `a := x; b := a*a+3` | `7` | `7` |
| `(p)` with `p = (x+1)^2; x = 3` | `16` | `16` |

* With several values (`x=1, y=2`), the values are computed first and
  then inserted all at the same time: `eval(x^2+y, x=y, y=3)` is `y^2+3`.
* Both also plug the current values into snapshots (`=` results with free
  symbols): `eval(p)` without values does only that.
* When the simplified expression still divides by something containing a
  variable, `eval` also cancels common factors (see `cancel`) before
  inserting the values: `eval((x^2-1)/(x-1), x=1)` is `2`, while `subs`
  gives a division by zero.

### Simplification

* **Evaluating simplifies automatically**, after values have been
  substituted:
  * like terms and like factors are combined: `2*x+3*x` → `5*x`,
    `x*x*y/x` → `x*y`, `x/x` → `1`, `x^3/x` → `x^2`, `2^a*2^b` → `2^(a+b)`
  * integer powers of products and of integer powers: `(2x)^2` → `4*x^2`,
    `(x^2)^3` → `x^6`
  * a number times a single sum is distributed: `2*(x+1)` → `2*x+2`
  * terms are ordered by total degree, then alphabetically, with the
    constant last: `y^2+x*y+x^2+1` → `x^2+x*y+y^2+1`
* Products of sums and powers of sums are **not** multiplied out
  automatically: `(x+2)^2` stays `(x+2)^2`. Use `expand` for that.
* Common factors of sums are **not** cancelled automatically:
  `(x^2-1)/(x-1)` stays as it is. Use `cancel` for that (it needs a
  polynomial GCD, which is too expensive to do on every result).
* **Storing with `:=`** keeps the formula as written, with only tidying
  that cannot change where it is defined: numbers, like terms
  (`2x+x` → `3*x`), equal powers (`x*x` → `x^2`) and numbers times sums.
  Nothing that removes a division is done: `x/x`, `x^2/x` and `1/x-1/x`
  (shown as `0/x`) are kept.
* **Roots are not combined for now**: powers of a symbol only merge for
  integer exponents. `(x^(1/2))^2`, `(x^2)^(1/2)`, `x^a*x^b` and
  `x^(1/2)*x^(1/2)` stay as they are. Simplifying roots correctly depends
  on which root is meant (and, with complex numbers, on the branch), which
  will be decided together with complex numbers.

### Comparisons and logic

`a == b` and `a != b` work on any expressions, including expressions
with free symbols. Internally, `a == b` evaluates both sides and computes
`expand(a - b)`. If that is the number `0`, they are equal. If it still
divides by something containing a variable, the difference is brought
to one fraction with common factors cancelled (as `cancel` does), and
they are equal when its numerator is `0`:

* For **polynomials and quotients of polynomials** (rational
  coefficients, any number of variables) this decides equality exactly:
  `(x+1)^2 == x^2+2x+1`, `(x^2-1)/(x-1) == x+1` and
  `1/x+1/y == (x+y)/(x*y)` are all true, whether or not the variables
  have values. Quotients are compared wherever both sides are defined.
* **Roots and symbolic powers** are treated like separate symbols, so
  `x^(1/2)*x^(1/2) == x` is false for now (see "Simplification").

`<`, `<=`, `>`, `>=` require both sides to evaluate to numbers; using
them on free symbols is an error. Comparisons give `1` (true) or `0`
(false). `&&` and `||` stop as soon as the result is known. Conditions
in `if`, `while` and `&&`/`||` must evaluate to numbers; anything except
`0` is true.

### Control flow

Bodies always need braces. `else if` can follow without extra braces, and
both `{` and `else` may start on a new line.

```
if (x < 0) { s = -1 } else if (x == 0) { s = 0 } else { s = 1 }
while (n > 1) { n = n/2; }
do { i = i+1; } while (i < 10)
for (i = 1 to 10) { disp(i^2) }
for (i = 10 to 1 step -3) { disp(i) }       # 10, 7, 4, 1
for (t = 0 to 1 step 1/4) { disp(t) }       # 0, 1/4, 1/2, 3/4, 1
for (i = 1; i*i < 10*i; i = i+1) { disp(i) }    # C style: 1, 2, ..., 9
```

Statements inside a body follow the normal output rule, so end them with
`;` to keep a loop quiet, and use `disp` to print.

`for (i = start to stop step s)` gives `i` the values `start`,
`start+s`, `start+2*s`, … and never goes past `stop`: it stops at
`<= stop` when `s > 0` and at `>= stop` when `s < 0`. The default step
is 1, `s = 0` is an error, and the bounds and step must be exact numbers
(fractions are fine). If `start` is already past `stop`, the body runs
zero times.

The C-style form `for (i = init; condition; i = next)` sets `i` to
`init`, then repeats: check `condition`, run the body, set `i` to `next`.
The step must assign the loop variable. The values can be anything (not
only numbers), but the condition must evaluate to a number.

When typing at a terminal, an `if` runs as soon as its block is closed.
An `else` typed on a later line then continues it: if a branch of the
`if` already ran, the `else` is skipped, otherwise it runs. This gives the
same result as when the whole statement is read at once:

```
a = 5;
if (a == 2)
{ b = 3; }           <- runs now: a is not 2, nothing happens
else { b = 10; }     <- continues the if: b = 10
```

In both loop forms, the loop variable is separate from any variable with
the same name. It
exists only in the text of the loop body, like a function parameter, and
cannot be assigned there:

```
i = 2;
for (i = 10 to 20) { }
disp(i)          -> 2
```

A `:=` formula or function that mentions the same name is ambiguous
inside the loop: does it mean the loop's `i` or the outer `i`? That is an
error rather than a silent guess:

```
a := i^2;
for (i = 1 to 3) { disp(a) }
-> error: a refers to i, which is also the loop variable here;
   use eval(a, i=i) for the loop value, or rename the loop variable
for (i = 1 to 3) { disp(eval(a, i=i)) }     # 1, 4, 9
f(k) := k^2;
for (i = 1 to 3) { disp(f(i)) }             # 1, 4, 9: parameters are clearest
```

A `:=` definition made inside a loop captures the current value of the
loop variable: after `for (i = 1 to 3) { a := x+i; }`, `a` is `x+3`.

An error inside a loop stops the whole loop and reports the line of the
statement that failed.

### Tables

`table(columns..., x = start to stop [step s], ...)` prints one line per
value, with the columns separated by tabs. Every argument written as
`name = ... to ...` is a range; all other arguments are columns:

```
table(x^2, x = 1 to 3)             table(x, x^2, x = 1 to 3)
1                                  1	1
4                                  2	4
9                                  3	9
```

With several ranges, the first one is the outer loop:

```
table(x, y, x*y, x = 1 to 2, y = 1 to 3)
1	1	1
1	2	2
1	3	3
2	1	2
2	2	4
2	3	6
```

Ranges follow the same `to` / `step` rules as `for`. The range variables
are temporary and are substituted first, like with `subs`, so a division
by zero shows up as an error rather than being simplified away. The output
pipes straight into gnuplot, `sort` or a spreadsheet.

### Built-in functions

| Function            | Meaning                                                    |
|---------------------|------------------------------------------------------------|
| `eval(e, x=v, ...)` | evaluates and simplifies `e` with `x` as a symbol, then inserts the values (all at once); also plugs current values into snapshots. Assigns nothing. |
| `subs(e, x=v, ...)` | substitutes the values first (all at once, also inside formulas), then evaluates and simplifies. Assigns nothing. |
| `expand(e)`         | multiplies out products and integer powers of sums          |
| `num(e[, digits])`  | decimal approximation of a number (default 20 digits, truncated) |
| `diff(e, x[, n])`   | n-th derivative with respect to `x` (default 1)             |
| `deg(e, x)`         | degree of the polynomial `e` in `x` (-1 for 0)              |
| `coeff(e, x, n)`    | coefficient of `x^n` (may contain other variables)          |
| `quo(p, q, x)`      | quotient of polynomial division in `x`                      |
| `rem(p, q, x)`      | remainder of polynomial division in `x`                     |
| `gcd(a, b, ...)`    | greatest common divisor of numbers or of polynomials        |
| `lcm(a, b, ...)`    | least common multiple of numbers or of polynomials          |
| `cancel(e)`         | `e` as one fraction with common factors cancelled           |
| `disp(e, ...)`      | prints the values, separated by tabs (statement only)       |
| `def(name)`         | prints how a variable or function is stored (statement only) |
| `unset(name, ...)`  | removes variables and functions (statement only)            |
| `table(...)`        | prints a table of values (statement only)                   |
| `save("file")`      | writes this session's statements to a file (statement only) |
| `transcript("file")`| the same, with each statement's output as `#` comments (statement only) |

In `diff`, `deg`, `coeff`, `quo` and `rem`, the variable argument is
treated as a free symbol even if it has a value. For example, after
`x=3`, `diff(x^2, x)` is `2*x`. Multivariate polynomials work everywhere:

```
expand((x+y)^3)                 -> x^3+3*x^2*y+3*x*y^2+y^3
coeff(3*x^2*y+x^2+5, x, 2)      -> 3*y+1
quo(x^3-y^3, x-y, x)            -> x^2+x*y+y^2
diff((x+1)^3, x)                -> 3*(x+1)^2
gcd(x^3-y^3, x^2-y^2)           -> x-y
```

**GCD and LCM.** For numbers, `gcd` and `lcm` work on integers and
fractions: `gcd(12, 18)` is `6`, and for fractions
`gcd(a/b, c/d) = gcd(a, c)/lcm(b, d)`, so `gcd(1/2, 3/4)` is `1/4`. As
soon as one argument is not a number, the arguments must be polynomials
(rational coefficients, any number of variables). The result is then a
polynomial over the rationals, where constant factors do not count: it is
scaled to integer coefficients without a common factor and a positive
first term. So `gcd(2x+4, 6x+12)` is `x+2` and `gcd(x^2+1, x+1)` is `1`.

**`cancel(e)`** brings `e` to one fraction `p/q` and removes the common
factors of `p` and `q` (using the polynomial GCD). The denominator gets
integer coefficients and a positive first term:

```
cancel((x^2-1)/(x-1))           -> x+1
cancel(1/x+1/y)                 -> (x+y)/(x*y)
cancel(1/(x-1)-1/(x+1))         -> 2/(x^2-1)
cancel((2x+2)/(4x+4))           -> 1/2
```

Roots and other non-polynomial parts are treated like symbols:
`cancel((x^(1/2)+1)^2/((x^(1/2)+1)*y))` is `(x^(1/2)+1)/y`. Cancelling can
make an expression defined where it was not (`x/x` at `x = 0`), so it is
never done automatically: stored `:=` formulas keep their factors, and
`subs` never cancels. `eval` cancels before inserting values.

### Saving your work

`save("work.xyz")` writes every statement run so far in this session
(typed, piped, from files or `-e`) to `work.xyz`, exactly as it was
written, so `xyz work.xyz` repeats the session. `transcript("work.txt")`
writes the same statements, each followed by the output it produced as
`#` comment lines: a readable record that can still be run with `xyz`.

```
a = 4
b = a*a+3;
b
1/0
transcript("work.txt")
```

writes

```
a = 4
# a = 4
b = a*a+3;
b
# 19
1/0
# error: line 4: division by zero
```

`save` and `transcript` themselves are not recorded, and an existing file
is overwritten. File names are written in double quotes; strings cannot
be used for anything else. Statements with a syntax error are not
recorded. Nothing is kept between sessions: there is no history file.

### Output format

The output can be read back as input: `x^2/4+x/3+1/9`, `-x/2`,
`3/(4*(x+y)^2)`, `x^(1/2)`, `2^(-x)`, `x < 3 && y == 2`. Results are
never broken into several lines, whatever their length (bc breaks lines at
70 characters with a `\`), so the terminal uses its full width.

## Planned

* Complex numbers. The imaginary unit will be `im` (as in Julia):
  `3+2im`, `im^2` → `-1`. That keeps `i`, `j` and `I` free for loops and
  other uses. The functions are `Re(z)`, `Im(z)`, `conj(z)` and `abs(z)`.
  Names are case-sensitive, so `Im` (the function) and `im` (the unit) are
  different names. Roots and complex branches will be decided together.
* `sin`, `cos`, `tan`, `atan`, `exp`, `ln`, `sqrt` and the constant `pi`:
  exact where possible (`sin(0)` → `0`, `sqrt(8)` → `2*sqrt(2)`), symbolic
  otherwise, with decimals from `num(sin(1), 50)`. These replace
  `bc -l`'s one-letter `s`, `c`, `a`, `l`, `e`.

## Tests

* `tests/cases/*.t`: golden input/output tests (`tests/run_tests.py`).
* `tests/cli_tests.py`: pipes, files, `-e`, options and exit status.
* `tests/random_tests.py`: randomised tests against independent oracles.
  Arithmetic is checked against Python's `fractions`, and big integers
  against the system `bc`. `expand`, `eval`, `subs`, `diff`, `quo` and `rem` are
  checked with algebraic identities and evaluation at random points.
  `gcd(A*C, B*C)` is checked to divide both arguments and to be a
  multiple of `C`, and `cancel` to keep the value at random points.
  `=` and `:=` are checked against a Python model of both semantics. A
  failure prints its seed (`--seed N`) so it can be reproduced.

## Source layout

| File                       | Contents                                         |
|----------------------------|--------------------------------------------------|
| `src/bigint.*`             | RAII wrapper around bcl numbers                  |
| `src/rational.*`           | exact rationals                                  |
| `src/expr.*`               | immutable, shared expression trees               |
| `src/simplify.cpp`         | simplification, expand, diff, polynomial helpers |
| `src/gcd.cpp`              | gcd/lcm of numbers and polynomials, cancel       |
| `src/print.cpp`            | expression → text                                |
| `src/parser.*`             | lexer and recursive-descent parser               |
| `src/interpreter.*`        | variables, functions, built-ins, statements      |
| `src/main.cpp`             | command line                                     |

## License

Copyright (C) 2026 Gunnar Lindholm. xyz is free software under the GNU
General Public License v3 or later (`LICENSE`).

The binary includes bcl from bc, which is under the BSD 2-Clause License
(and the MIT License for `rand.c`). Both are GPL-compatible. Their
notices must be distributed with the binary. See `LICENSES/`.
