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
xyz [-h] [-v] [--imag=NAME] [--trunc] [-e expr]... [file]...
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
* `--imag=NAME` calls the imaginary unit `NAME` instead of `im`, for input
  and output: with `--imag=i`, `(3+2i)*(1-i)` gives `5-i`, and `im` is an
  ordinary name. `i` can then not be used as a variable or loop variable.
* `--trunc` makes `num()` truncate decimals like bc instead of rounding.
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
sqrt(18)         -> 3*2^(1/2) (square roots of numbers are taken exactly)
sqrt(-4)         -> 2*im
1/0              -> error: division by zero
0^0              -> 1        (as in bc)
```

Results are never rounded. `num(e, digits)` shows a decimal value with
`digits` decimals (default 20), rounded (or truncated like bc with
`--trunc`):

```
num(1/3)               -> 0.33333333333333333333
num(2/3, 5)            -> 0.66667
num(pi, 50)            -> 3.14159265358979323846264338327950288419716939937511
num(sqrt(2) + sin(1))  -> 2.25568454718099155545
num(exp(2im), 10)      -> -0.4161468365+0.9092974268*im
num(x/3 + sqrt(2), 5)  -> 0.33333*x+1.41421     (symbols are kept)
```

Fractions are rounded exactly. Roots, `pi` and the functions are computed
with bcl at extra precision (20 guard digits, more where a function needs
it) and then rounded. Integers stay as they are (`num(2^100)`). Values with
more than 100000 digits are refused. The result of `num` is for display
only and cannot be used in further calculations.

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

### Lists

`[a, b, c]` is a list, and `l[k]` its item number `k` (from 1). Lists can
hold anything, also lists: `[[1, 2], [3, 4]][2][1]` is `3`.

```
l = [1, 2, x]
l[3]                    -> x
length(l)               -> 3
l + 1                   -> [2, 3, x+1]
2*l, l^2, l/2           -> element by element
l + [10, 20, 30]        -> [11, 22, x+30]
sin([0, pi/2])          -> [0, 1]
diff([x^2, sin(x)], x)  -> [2*x, cos(x)]
seq(i^2, i = 1 to 5)    -> [1, 4, 9, 16, 25]
sum(i, i = 1 to 100)    -> 5050
sum([1, 2, 3])          -> 6
[1, 2] == [1, 2]        -> 1
```

* `+ - * / ^` work element by element. A number or expression is used with
  every element; two lists must have the same length.
* Functions apply to each element: the elementary functions, `expand`,
  `num`, `diff`, `integrate`, `factor`, `cancel`, `sqrt`, `Re`, `Im`,
  `abs`, `deg`, `coeff`, `trigexpand`, `trigreduce`, `taylor`, `limit`.
* `seq(e, i = a to b [step s])` makes a list, and `sum(e, i = a to b)`
  adds the values (the range is written as in `table`). `sum(list)` adds
  the elements.
* `==` compares lists element by element. `<` and the like need numbers.
* Lists can be stored and returned by functions (`f(t) = [t, t^2]`).
  Items cannot be assigned (`l[2] = 5` is not possible).

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
* **Roots of symbols** are combined only where it is always true:
  `(x^2)^(1/2)` → `abs(x)` (symbols are real), `abs(x)^2` → `x^2`.
  `(x^(1/2))^2`, `x^a*x^b` and `x^(1/2)*x^(1/2)` stay as they are. With
  `assume(x >= 0)` (see "Assumptions"), `(x^a)^b` → `x^(a*b)`.
* **Roots of numbers**: square roots are taken exactly as far as square
  factors can be found: `sqrt(8)` → `2*2^(1/2)`, `sqrt(2/9)` → `2^(1/2)/3`,
  and `sqrt(-4)` → `2*im` (the principal value). Powers of positive numbers
  combine: `(2^(1/2))^2` → `2`, `2^a*2^b` → `2^(a+b)`. Other roots of
  numbers stay as they are: `8^(1/3)`.

### Complex numbers

`im` is the imaginary unit (as in Julia; `i`, `j` and `I` stay free for
loops and other uses). Start xyz with `--imag=i` to call it `i` instead.
It cannot be assigned. `im^2` is `-1`, and **all symbols are taken to be
real**.

* Exact complex numbers are always brought to the form `a+b*im`, like
  fractions are always reduced:

  ```
  (3+im)*(5-3im)     -> 18-4*im
  1/(1+im)           -> 1/2-im/2
  (1+im)^10          -> 32*im
  ```
* With symbols, products of sums are kept, as for real sums, and `expand`
  multiplies them out: `expand((x+im)^2)` → `x^2+2*im*x-1`.
* `Re(z)`, `Im(z)`, `conj(z)` and `abs(z)` split `z` into its real and
  imaginary parts. A quotient is made real by multiplying with the
  conjugate of its denominator:

  ```
  Re(x+im*y)         -> x
  Im((x+im*y)^2)     -> 2*x*y
  Re(1/(x+im))       -> x/(x^2+1)
  conj(1/(1+im))     -> 1/2+im/2
  abs(3+4im)         -> 5
  abs(x+im*y)        -> (x^2+y^2)^(1/2)
  ```
* When the parts cannot be found, the function stays as it is (and is
  evaluated again when values are put in later):

  ```
  Re(x^(1/2))                  -> Re(x^(1/2))    (x could be negative)
  eval(Re(x^(1/2)), x=-4)      -> 0
  abs(x)                       -> abs(x)
  ```
  The parts can be found for numbers, `im`, symbols, sums, products,
  integer powers, positive numbers to real powers (`2^(1/2)`, `2^x`) and
  `Re`, `Im`, `abs`, `conj` themselves.
* `num(1/3+2im/3, 5)` → `0.33333+0.66666*im`.
* `<`, `<=`, `>` and `>=` need real numbers. `==` works: `(1+im)^2 == 2im`
  is true.

### Elementary functions

`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `ln`, `log(b, x)` (and
`log(x)` = `ln(x)`), the hyperbolic functions `sinh`, `cosh`, `tanh`,
`asinh`, `acosh`, `atanh`, with the constant `pi` (which cannot be
assigned).
Angles are in radians. There is no constant `e`: write `exp(1)`. They are
kept basic: exact where a standard value is known, otherwise they stay as
they are (`sin(1)`, `ln(x)`), and `num` gives decimals.

* **Exact values** at multiples of `pi/6` and `pi/4`, and the matching
  inverse values:

  ```
  sin(pi/6)          -> 1/2
  cos(3pi/4)         -> -2^(1/2)/2
  tan(pi/3)          -> 3^(1/2)
  asin(1/2)          -> pi/6
  atan(-1)           -> -pi/4
  tan(pi/2)          -> error: tan(pi/2) is undefined
  ```
* **Symmetries and periods:** `sin(-x)` → `-sin(x)`, `cos(-x)` → `cos(x)`,
  `sin(x+pi)` → `-sin(x)`, `cos(x+2pi)` → `cos(x)`, `sin(pi-x)` → `sin(x)`,
  `acos(-x)` → `pi-acos(x)`.
* **exp and ln:** `exp(0)` → `1`, `ln(1)` → `0`, `exp(ln(x))` → `x`,
  `ln(exp(x))` → `x` (symbols are real), `exp(a)*exp(b)` → `exp(a+b)`,
  `exp(x)^n` → `exp(n*x)`, `ln(1/3)` → `-ln(3)`, `ln(-2)` → `ln(2)+im*pi`
  (principal value), `exp(pi*im)` → `-1`, `exp(pi*im/3)` → `1/2+3^(1/2)*im/2`.
  `exp(2im)` stays as it is (`Re`, `Im`, `conj` and `abs` use Euler's
  formula: `Re(exp(2im))` → `cos(2)`).
* **log:** `log(2, 8)` → `3`, `log(4, 2)` → `1/2`, otherwise
  `log(b, x)` → `ln(x)/ln(b)`.
* **Inverses:** `sin(asin(x))` → `x`, `cos(acos(x))` → `x`,
  `tan(atan(x))` → `x`. (`asin(sin(x))` is only `x` for some `x`, so it
  stays.)
* **Hyperbolic functions:** values at 0 (`cosh(0)` → `1`), symmetries
  (`sinh(-x)` → `-sinh(x)`, `cosh(-x)` → `cosh(x)`), inverses
  (`sinh(asinh(x))` → `x`), `c*cosh(u)^2 - c*sinh(u)^2` → `c`,
  derivatives and decimals. `Re`, `Im` use them for complex arguments:
  `Re(sin(im*x))` → `0`, `Im(sin(im*x))` → `sinh(x)`.
* **Integer multiples of pi** (with `assume(n, integer)`): `sin(n*pi)` →
  `0`, `cos(n*pi)` → `(-1)^n`, `sin(x + 2*n*pi)` → `sin(x)`.
* **sin² + cos²:** `c*sin(u)^2 + c*cos(u)^2` → `c` in sums, also with other
  factors: `3*sin(x)^2 + 3*cos(x)^2 + 1` → `4`. More is done only on
  request, with `trigexpand` and `trigreduce`.
* **Derivatives:** `diff` knows all of them, with the chain rule, and
  powers with the variable in the exponent: `diff(2^x, x)` → `2^x*ln(2)`,
  `diff(x^x, x)` → `x^x*(ln(x)+1)`.
* **`trigexpand(e)`** applies the addition formulas and writes
  `sin(n*x)`, `cos(n*x)` (integer `n`) in `sin(x)` and `cos(x)`, and
  `tan(x+y)`, `tan(n*x)` in `tan(x)` and `tan(y)`:

  ```
  trigexpand(sin(2x))      -> 2*cos(x)*sin(x)
  trigexpand(cos(x+y))     -> cos(x)*cos(y)-sin(x)*sin(y)
  trigexpand(cos(3x))      -> cos(x)^3-3*cos(x)*sin(x)^2
  trigexpand(tan(x+y))     -> (tan(x)+tan(y))/(-tan(x)*tan(y)+1)
  trigexpand(tan(2x))      -> 2*tan(x)/(-tan(x)^2+1)
  trigexpand(tan(x+pi/2))  -> -1/tan(x)
  ```
* **`trigreduce(e)`** goes the other way: products and powers of `sin` and
  `cos` become sums of `sin` and `cos` of combined angles (product-to-sum
  formulas, and the binomial theorem for powers). That also undoes
  `trigexpand`. With `tan`, or `sin`/`cos` in a denominator, `tan` is
  written as `sin/cos`, the expression is made one fraction, numerator and
  denominator are reduced on their own, and `sin(v)/cos(v)` becomes
  `tan(v)`, also from the half-angle forms `sin(2v)/(1+cos(2v))` and
  `(1-cos(2v))/sin(2v)`. For a sum, this is also done term by term, and
  the shorter result is kept. Powers above 1000 are refused.

  ```
  trigreduce(2*sin(x)*cos(x))           -> sin(2*x)
  trigreduce(cos(x)^2-sin(x)^2)         -> cos(2*x)
  trigreduce(cos(x)*cos(y)-sin(x)*sin(y)) -> cos(x+y)
  trigreduce(sin(x)^3)                  -> 3*sin(x)/4-sin(3*x)/4
  trigreduce(trigexpand(cos(3x)))       -> cos(3*x)
  trigreduce(2*tan(x)/(1-tan(x)^2))     -> tan(2*x)
  trigreduce(tan(x)^2+1)                -> 2/(cos(2*x)+1)
  trigreduce(sin(x)/(1+cos(x)))         -> tan(x/2)
  ```

### Factoring

`factor(e)` writes a number as a product of primes, and a polynomial (or a
fraction of polynomials) as a product of factors with rational
coefficients. The result is shown factored, but it computes like the
value it stands for: `f = factor(120)` shows `2^3*3*5`, and `f + 1` is
`121`.

```
factor(120)                 -> 2^3*3*5
factor(-12/35)              -> -2^2*3/(5*7)
factor(2^64+1)              -> 274177*67280421310721
factor(12x^2-12)            -> 12*(x-1)*(x+1)
factor(x^4+4)               -> (x^2-2*x+2)*(x^2+2*x+2)
factor(x^6-1)               -> (x-1)*(x+1)*(x^2+x+1)*(x^2-x+1)
factor((x^2-1)/(x^2+2x+1))  -> (x-1)/(x+1)
factor(x^3+y^3)             -> (x+y)*(x^2-x*y+y^2)
factor(sin(x)^2-1)          -> (sin(x)-1)*(sin(x)+1)
factor(x^2+(a+b)*x+a*b)     -> (a+x)*(b+x)
factor(x^2+2*sqrt(2)*x+2)   -> (x+2^(1/2))^2
```

* **Numbers** are divided by the primes up to 10^5. What is left is
  tested with Miller-Rabin and split with Pollard's rho method (Brent's
  variant). Every number below 2^63 is factored completely. Numbers below
  2^126 (38 digits) are done with 128-bit Montgomery arithmetic. There,
  any number whose prime factors, except the largest, are below about
  10^15 is factored in a few seconds. Larger numbers use bcl and find
  prime factors up to about 10^10. Otherwise rho gives up after a fixed
  number of steps (about 5 seconds) with an error. Numbers with more than
  300 digits are refused. Below 3.3·10^24 the primality test is exact.
  Above that, a large factor is a probable prime (12 or 16 Miller-Rabin
  bases).
* **Polynomials in one variable** are split into square-free parts
  (Yun's algorithm), then rational roots are found (`p/q` with `p`
  dividing the constant term and `q` the leading coefficient). The rest is
  searched for factors of degree 2 and more by Kronecker's method, which
  gives up when there are more than 20000 candidates. So small polynomials
  are factored completely, and large ones may keep a factor that could be
  split further.
* **Several variables:** numbers and common factors are taken out for
  each variable (`a*x+a*b` → `a*(b+x)`, `x^2*y-y+x^2-1` →
  `(x-1)*(x+1)*(y+1)`), then the square-free parts. Homogeneous
  polynomials in two variables are factored like polynomials in one
  (`x^2-y^2` → `(x+y)*(x-y)`). Then factors that are linear in one of the
  symbols are searched for: `d1*v + d2` with `d1` dividing the leading
  coefficient in `v` and `d2` the constant term (both factored first),
  which finds `x^2+(a+b)*x+a*b` → `(a+x)*(b+x)`. Other factors in several
  variables are kept as they are.
* Factors are written with integer coefficients and a positive leading
  coefficient. The number in front takes the rest. Calls and roots are
  treated like variables (`sin(x)` above).
* **Roots in the coefficients:** a quadratic in one symbol whose
  coefficients have roots is split when its discriminant is 0 or the
  square of a rational number: `x^2+2*sqrt(2)*x+2` → `(x+2^(1/2))^2`,
  `x^2-2*sqrt(2)*x+1` → `(x-2^(1/2)-1)*(x-2^(1/2)+1)`. Other factoring
  over extensions is not done (`x^2-2` stays).

### Solving equations

`solve(eq, x)` solves the equation `eq` for `x`. `eq` is `a = b`, or an
expression meaning `expr = 0`. `x` can be left out when the equation has
only one symbol. The solutions are listed, each one once, rational ones
first and ascending. `solve(eq, x, k)` gives solution number `k` as a
value to compute with.

```
solve(x^2 = 4)              -> x = -2, x = 2
solve(x^2+x+1)              -> x = -1/2+3^(1/2)*im/2, x = -1/2-3^(1/2)*im/2
solve(a*x^2+b*x+c, x)       -> x = (-b+(-4*a*c+b^2)^(1/2))/(2*a), x = (-b-(-4*a*c+b^2)^(1/2))/(2*a)
solve(x^3-6x^2+11x-6)       -> x = 1, x = 2, x = 3
solve(x^3-2)                -> x = 2^(1/3), x = 2^(1/3)*(-1/2+3^(1/2)*im/2), ...
solve(x^3-3x+1)             -> x = 2*cos(2*pi/9), x = 2*cos(4*pi/9), x = 2*cos(10*pi/9)
solve(x^4-10x^2-8x+5)       -> x = 6^(1/2)+1, x = -6^(1/2)+1, x = 2^(1/2)-1, x = -2^(1/2)-1
solve(x^7-x)                -> x = -1, x = 0, x = 1, ... (and the four complex ones)
solve(x^5-x-1)              -> x^5-x-1 = 0          (not solved)
solve((x^2-1)/(x-1))        -> x = -1               (x = 1 makes the denominator 0)
r = solve(x^3-2, x, 1)      -> r = 2^(1/3)
s = solve(x^2-4); s[2]      -> 2           (the result computes as the list [-2, 2])
```

* Only polynomial equations are solved (also fractions of polynomials:
  the numerator is solved, and roots of the denominator are left out).
  Equations with `sin(x)`, `exp(x)`, `x^(1/2)`, ... in `x` give an error.
* The polynomial is factored first (see `factor`), and each factor is
  solved by formula:
  - degree 1 and 2, also with symbols as coefficients;
  - degree 3 by Cardano's formula. With three real roots the
    trigonometric form is used, which avoids cube roots of complex
    numbers;
  - degree 4 by Ferrari's method. When its resolvent cubic has a rational
    root the result is short. Otherwise it is long, but exact.
  Degree 3 and 4 need numbers as coefficients.
* Factors that are not solved are kept in the list as `factor = 0`. These
  are factors of degree 5 and more that do not split further, and factors
  of degree 3 and 4 with symbols. `num()` of a solution gives decimals.
* Factors without `x` (like `a` in `a*x+a*b`) are taken to be nonzero.
  `solve(a*x+b, x)` gives `-b/a` without asking whether `a = 0`.
* The result is shown as `x = ...`, but it computes as the list of the
  solutions: `s = solve(x^2-4)` shows `x = -2, x = 2`, and `s[1]` is `-2`,
  `length(s)` is `2`, `s + 1` is `[-1, 3]`. When factors are left unsolved
  there is no list; `solve(eq, x, k)` still gives solution `k`.
  `solve(1/x)` gives `no solution` (the empty list), `solve(x-x)` gives
  `every value is a solution`.
* With assumptions about `x` (see "Assumptions") only the solutions that
  fit them are kept: after `assume(x > 0)`, `solve(x^2 = 4)` gives `x = 2`.

**Systems of equations**: a list of equations, and a list of the symbols
to solve for (or none, for all the symbols in the equations):

```
solve([x+y = 3, x-y = 1], [x, y])   -> x = 2, y = 1
solve([x+y+z = 6, x-y = 0, y-z = 0]) -> x = 2, y = 2, z = 2
solve([a*x+b*y = c, x-y = 0], [x, y]) -> x = c/(a+b), y = c/(a+b)
solve([x+y = 1, 2x+2y = 2], [x, y]) -> x = -y+1          (y is free)
solve([x+y = 1, x+y = 2], [x, y])   -> no solution
solve([x*y = 1, x+y = 2])           -> x = 1, y = 1
solve([x^2+y^2 = 25, x-y = 1])      -> x = -3, y = -4; x = 4, y = 3
```

* **Linear systems** are solved by Gaussian elimination with exact
  coefficients (also with symbols as coefficients; dividing by them takes
  them to be nonzero). The result computes as the list of the values, one
  per symbol: free symbols stay as they are (`s[2]` above is `y`).
* **Other systems** are solved by elimination. An equation that is linear
  in one symbol is solved for it and put into the others. Otherwise one
  with only one of the symbols is solved (as above) and each of its roots
  is followed. Solution sets are separated by `;`. The result computes as
  a list of lists, one per solution set. Systems where neither step is
  possible give an error.

### Calculus: integrate, taylor, limit

```
integrate(x^2*sin(x), x)        -> -x^2*cos(x)+2*x*sin(x)+2*cos(x)
integrate(1/(x^3+1), x)         -> 3^(1/2)*atan(...)/3+ln(x+1)/3-ln(x^2-x+1)/6
integrate(1/sqrt(1-x^2), x)     -> asin(x)
integrate(exp(x)*sin(x), x)     -> (-cos(x)+sin(x))*exp(x)/2
integrate(x*exp(x^2), x)        -> exp(x^2)/2
integrate(sin(x)^2, x)          -> x/2-sin(2*x)/4
integrate(exp(x^2), x)          -> integrate(exp(x^2), x)   (not found)
integrate(x^2, x, 0, 1)         -> 1/3
integrate(exp(-x), x, 0, inf)   -> 1
integrate(ln(x), x, 0, 1)       -> -1
taylor(sin(x), x, 7)            -> -x^7/5040+x^5/120-x^3/6+x
taylor(ln(x), x = 1, 3)         -> (x-1)^3/3-(x-1)^2/2+x-1
limit(sin(x)/x, x = 0)          -> 1
limit((1+1/x)^x, x = inf)       -> exp(1)
limit(1/x, x = 0, right)        -> inf
limit(sqrt(x^2+x)-x, x = inf)   -> 1/2
```

* **`integrate(f, x)`** gives an antiderivative (without a constant).
  `integrate(f, x, a, b)` gives `F(b) - F(a)`. The methods are tried in
  turn. It is not the Risch algorithm, so some integrals that exist are
  not found:
  - linearity, and a table: powers, `exp`, `ln`, `sin`, `cos`, `tan`,
    the inverse and hyperbolic functions, `abs`, `1/cos^2`, `1/sin`,
    `tan^2`, all with linear arguments (`sin(2x+1)`);
  - rational functions: polynomial division and partial fractions over
    the factors found by `factor` (linear factors of any multiplicity,
    quadratic ones with a reduction formula). Factors of degree 3 and
    more that do not split are not integrated;
  - `sqrt` of a quadratic and `1/sqrt` of a quadratic (`asin`, `asinh`,
    `acosh`, `ln`);
  - integration by parts: a polynomial times `exp`, `sin`, `cos`, `sinh`,
    `cosh`, `c^x`, or times `ln`, `asin`, `acos`, `atan`, ...;
    `exp(ax)*sin(bx)` and `exp(ax)*cos(bx)`;
  - substitution: `f(u(x))*u'(x)` for the parts `u` of the integrand
    (`x*exp(x^2)`, `ln(x)/x`, `x*sqrt(x+1)`, `1/(1+exp(x))`);
  - products and powers of `sin` and `cos` (with `trigreduce`).
  Logarithms are written without absolute values: `integrate(1/x, x)` is
  `ln(x)`. What is not found stays as `integrate(f, x)`, also for parts of
  a sum. Its derivative is `f`.
* **Definite integrals** use limits at `inf`, `-inf`, and at bounds where
  the antiderivative is not defined (`ln(0)` above). A divergent integral
  gives `inf` or `-inf`. A pole between the bounds (a real root of the
  denominator) gives an error: `integrate(1/x^2, x, -1, 1)`. Other
  singularities between the bounds (of `tan`, `ln`, ...) are not checked.
* **`taylor(f, x = a, n)`** is the Taylor polynomial of degree `n` at `a`
  (`taylor(f, x, n)` at 0; `n` is 5 when left out). It is made from the
  derivatives, so it needs `f` to be differentiable at `a`
  (`taylor(ln(x), x = 0, 3)` gives an error).
* **`limit(f, x = a)`**, and `limit(f, x = a, left)` or `right`. `a` may
  be `inf` or `-inf`, and the result may be `inf` or `-inf`. Where `f` is
  defined it is its value. Otherwise the limits of the parts are
  combined, with L'Hôpital's rule for 0/0 and inf/inf, `0*inf` rewritten
  as a quotient, `b^e` as `exp(e*ln(b))`, `A - B` with a square root as
  `(A^2-B^2)/(A+B)`, and the known limits of the functions
  (`ln(0+) = -inf`, `atan(inf) = pi/2`). At `inf`, powers and roots of
  polynomials go by their leading terms. The sign of an infinite limit is
  found numerically, close to the point. When the two sides differ, or no
  rule applies, it gives an error (`limit(sin(x), x = inf)`).
* `inf` is a constant name for these functions. It can be used in limits
  and integral bounds, and comes out of them, but arithmetic with it is
  not defined: `inf + 1` stays `inf+1`.

### Assumptions

`assume` tells what is known about a symbol. It is a statement on its
own:

```
assume(x > 0)         also >=, <, <=, != (and 0 < x)
assume(n, integer)
assumptions()         -> lists them: x > 0, n is an integer
forget(x)             -> removes what is known about x
```

What follows from them:

* `sqrt(x^2)` → `x`, `abs(x)` → `x` for `x >= 0` (and `-x` for `x <= 0`),
  `(x^a)^b` → `x^(a*b)` for `x >= 0`.
* `ln(a*b)` → `ln(a)+ln(b)` when every factor is known to be positive
  (`ln(2x)` → `ln(2)+ln(x)`), and `ln(x^r)` → `r*ln(x)` for `x > 0`.
* `assume(x != 0)` lets formulas stored with `:=` cancel `x`:
  `f := x/x` stores `1` (without it, `x/x` is kept there, because
  cancelling would change where the formula is defined). `solve` then
  leaves out `x = 0`.
* For integers: `sin(n*pi)` → `0`, `cos(n*pi)` → `(-1)^n`,
  `sin(x + 2*n*pi)` → `sin(x)`, `(-1)^(2n)` → `1`.
* `solve` keeps only the solutions that fit (real and with the right
  sign, integer).

When typing at a terminal, xyz says once per session when a result could
be simplified with an assumption, for example:

```
f := x/x
note: x/x would be 1 with assume(x != 0) (this note is shown once)
```

It looks for `abs(x)`, `ln(x*y)`, `ln(x^r)`, `(x^a)^b`, `sin(n*pi)` and
`cos(n*pi)`, and `x/x` kept in a formula, with symbols that have no
assumption yet. The note goes to stderr, and is not shown when reading
from a pipe or a file, or in `save` and `transcript` output.

Signs of expressions are worked out from the assumptions: sums and
products of positive terms are positive, even powers of real values are
`>= 0`, `exp` of a real value is positive. All symbols are real in any
case. Assumptions are part of the session (`save` writes the `assume`
statements); `unset` does not remove them, `forget` does.

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
* **Lists** are equal when they have the same length and equal elements.

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
| `num(e[, digits])`  | decimal value, rounded to `digits` decimals (default 20); numbers inside expressions with symbols are converted |
| `diff(e, x[, n])`   | n-th derivative with respect to `x` (default 1)             |
| `deg(e, x)`         | degree of the polynomial `e` in `x` (-1 for 0)              |
| `coeff(e, x, n)`    | coefficient of `x^n` (may contain other variables)          |
| `quo(p, q, x)`      | quotient of polynomial division in `x`                      |
| `rem(p, q, x)`      | remainder of polynomial division in `x`                     |
| `gcd(a, b, ...)`    | greatest common divisor of numbers or of polynomials        |
| `lcm(a, b, ...)`    | least common multiple of numbers or of polynomials          |
| `cancel(e)`         | `e` as one fraction with common factors cancelled           |
| `sqrt(e)`           | `e^(1/2)`; exact for numbers where possible, `sqrt(-4)` → `2*im` |
| `sin`, `cos`, `tan` | trigonometric functions (radians)                           |
| `asin`, `acos`, `atan` | inverse trigonometric functions                          |
| `exp(e)`, `ln(e)`   | exponential function and natural logarithm                  |
| `log(b, x)`         | logarithm to base `b`; `log(x)` is `ln(x)`                  |
| `trigexpand(e)`     | addition and multiple-angle formulas                        |
| `trigreduce(e)`     | products and powers of sin/cos as sums of sin/cos           |
| `sinh`, `cosh`, `tanh` | hyperbolic functions                                     |
| `asinh`, `acosh`, `atanh` | inverse hyperbolic functions                          |
| `factor(e)`         | primes of a number, factors of a polynomial or fraction     |
| `solve(eq, x[, k])` | solutions of a polynomial equation (`k`: only solution `k`) |
| `solve([eqs], [xs])` | a system of equations                                      |
| `integrate(f, x[, a, b])` | antiderivative, or the integral from `a` to `b`       |
| `taylor(f, x = a, n)` | Taylor polynomial of degree `n` at `a`                    |
| `limit(f, x = a[, left\|right])` | limit, also at `inf` and `-inf`                 |
| `length(l)`         | number of elements of a list                                |
| `seq(e, i = a to b)` | list of the values of `e`                                  |
| `sum(e, i = a to b)`, `sum(l)` | sum of the values, or of a list                  |
| `assume(x > 0)`, `assume(n, integer)` | what is known about a symbol (`> >= < <= !=`; statement only) |
| `forget(x)`, `assumptions()` | removes / lists assumptions (statement only)       |
| `Re(z)`, `Im(z)`    | real and imaginary part (symbols are real)                  |
| `conj(z)`           | complex conjugate                                           |
| `abs(z)`            | absolute value                                              |
| `disp(e, ...)`      | prints the values, separated by tabs (statement only)       |
| `def(name)`         | prints how a variable or function is stored (statement only) |
| `unset(name, ...)`  | removes variables and functions (statement only)            |
| `table(...)`        | prints a table of values (statement only)                   |
| `latex(e)`          | prints the value of `e` as LaTeX math (statement only)      |
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

**LaTeX.** When the file name ends in `.tex`, `transcript` writes a LaTeX
document instead: every statement verbatim, followed by its results as
formulas (fractions, roots, `i` for the imaginary unit, tables as arrays,
errors in italics). `latex(e)` prints a single value as LaTeX math:

```
latex((x+1)^2/3)       -> \frac{\left(x+1\right)^{2}}{3}
latex(sqrt(8)/x)       -> \frac{2 \sqrt{2}}{x}
latex(alpha_1 + abs(z)) -> \alpha_{1}+\left|z\right|
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

## Not solved yet

Things that are known to be missing or only partly done:

* **Factoring and solving** are kept simple. Factoring is over the
  rationals (and the roots already in the coefficients of a quadratic),
  and fully only in one variable. Kronecker's method gives up on large
  polynomials, and Pollard's rho on numbers with two large prime factors
  (see `factor`). `solve` only handles polynomial equations, solves degree
  5 and more only through factors, gives the general quartic as a long
  exact formula, and solves systems only by linear algebra and simple
  elimination.
* **Lists** cannot be changed in place (`l[2] = 5`), and there are no
  matrices: `*` on lists is element by element.

* **Assumptions** are about signs and integers of single symbols only.
  There are no intervals (`asin(sin(x))` → `x` needs `-pi/2 <= x <= pi/2`),
  no complex symbols, and nothing like `assume(x > y)`. Signs of
  expressions are only found for simple combinations.
* **Calculus.** `integrate` is a set of methods, not a decision procedure
  (`exp(x^2)` has no elementary antiderivative, but others that have one
  may be missed). Definite integrals check only the poles of the
  denominator between the bounds. `limit` gives up where its rules do not apply (oscillation,
  nested limits that L'Hôpital cannot shorten). There are no series beyond
  Taylor polynomials, and no differential equations.
* **Roots of symbols.** `(x^(1/2))^2` is not simplified to `x`, and
  `x^a*x^b` is not combined, even though both are true for the principal
  values.
* **Which square root?** `sqrt(-4)` gives `2*im`, the principal value
  (the root with positive real part, or positive imaginary part for
  negative numbers). This is what Mathematica, Maple, Julia, MATLAB and
  Python do, but `-2*im` is just as much a square root of `-4`. The
  convention is kept for now and may be revisited together with roots of
  symbols.
* **Roots of numbers** other than square roots are not computed:
  `8^(1/3)` stays as it is. Square roots only find small square factors
  of large numbers (factoring large numbers is hard).
* **Splitting into real and imaginary parts** fails for roots of symbols
  and unknown functions: `Re(x^(1/2))` stays as it is.
* **GCD over the complex numbers.** `gcd` and `cancel` treat `im` like an
  extra symbol, so `gcd(x^2+1, x-im)` is `1`, although `x^2+1 = (x-im)(x+im)`.
* **`==` with roots or unknown functions** can be false for equal
  expressions: `x^(1/2)*x^(1/2) == x` is false.
* **Complex arguments of `asin`, `acos`, `atan`**, and `asin(2)` (which is
  complex): no exact or decimal value yet, they stay as they are.
* **Inverse of a function:** `asin(sin(x))` is `x` only for
  `-pi/2 <= x <= pi/2`, so it is not simplified (assumptions have no
  intervals).
* **Logarithm rules** `ln(a*b)` → `ln(a)+ln(b)` and `ln(x^n)` → `n*ln(x)`
  are only done when the factors are assumed positive.
* **Automatic trigonometric simplification** beyond `sin^2+cos^2 = 1` is
  not done: `trigexpand` and `trigreduce` must be asked for, and nothing
  picks the shortest form by itself.
* **`num` has no proven error bounds.** The guard digits make a wrong
  last digit very unlikely, but not impossible. Numbers with more than
  100000 digits are refused.

## Tests

* `tests/cases/*.t`: golden input/output tests (`tests/run_tests.py`).
* `tests/cli_tests.py`: pipes, files, `-e`, options and exit status.
* `tests/random_tests.py`: randomised tests against independent oracles.
  Arithmetic is checked against Python's `fractions`, and big integers
  against the system `bc`. `expand`, `eval`, `subs`, `diff`, `quo` and `rem` are
  checked with algebraic identities and evaluation at random points.
  `gcd(A*C, B*C)` is checked to divide both arguments and to be a
  multiple of `C`, and `cancel` to keep the value at random points.
  Complex arithmetic and `Re`/`Im`/`conj` are checked against exact
  complex fractions, and `Re(e) + im*Im(e) == e` for expressions with
  symbols. `num()` of random expressions with roots, `pi`, `exp`, `ln`,
  `sin`, `cos` and `atan` is checked against Python's `decimal` module (with
  its own series for the trigonometric functions), `diff` of random
  functions against central differences, and `trigexpand` and
  `trigreduce` by value. `factor` of random integers is checked to give
  ascending primes whose product is the number. `factor` of random
  products of polynomials is checked by value. `solve` is checked by
  putting every solution back into the polynomial (with `num`) and by
  counting the solutions. Linear systems with a known solution are solved
  and compared. `integrate` of random integrands is checked by
  differentiating the result, and `taylor` by comparing with the function
  close to the point.
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
| `src/complex.cpp`          | Re, Im, conj, abs                                |
| `src/elem.cpp`             | elementary and hyperbolic functions, trigexpand, trigreduce |
| `src/factor.cpp`           | factor(): primes, polynomial factorization       |
| `src/solve.cpp`            | solve(): polynomial equations, systems           |
| `src/integrate.cpp`        | integrate()                                      |
| `src/limit.cpp`            | limit(), taylor()                                |
| `src/assume.cpp`           | assumptions, known signs                         |
| `src/numeric.cpp`          | decimal values for num()                         |
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
