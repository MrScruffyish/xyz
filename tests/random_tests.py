#!/usr/bin/env python3
# Randomised tests for xyz against independent oracles.
# Copyright (C) 2026 Gunnar Lindholm
# SPDX-License-Identifier: GPL-3.0-or-later
"""
  1. exact rational arithmetic vs Python's fractions.Fraction
  2. big-integer arithmetic vs the system bc (if installed)
  3. expand(): result is a polynomial and equals the input at random points
  4. eval(e, x=.., y=..) vs Python evaluation
  5. diff() commutes with expand()
  6. quo/rem satisfy  A = Q*B + R  with deg R < deg B
     gcd(A*C, B*C) divides both and is a multiple of C; cancel keeps values
  7. complex arithmetic and Re/Im/conj vs exact complex fractions
     num() vs Python's decimal module; diff vs central differences;
     trigexpand keeps the value
  8. '=' snapshots and ':=' formulas vs a Python model

usage: random_tests.py [binary] [--count N] [--seed S]
"""
import argparse
import random
import re
import shutil
import subprocess
import sys
from fractions import Fraction as F

failures = 0


def xyz(binary, lines):
    p = subprocess.run([binary], input="\n".join(lines) + "\n",
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, timeout=600)
    return p.stdout.rstrip("\n").split("\n")


def fail(what, detail):
    global failures
    failures += 1
    if failures <= 20:
        print(f"FAIL {what}: {detail}")


def fstr(v):
    return str(v)  # Fraction prints as p/q or p, same as xyz


# ------------------------------------------------------------ numbers

class DivZero(Exception):
    pass


def gen_num(rng, depth):
    if depth == 0 or rng.random() < 0.25:
        r = rng.random()
        if r < 0.6:
            v = rng.randint(-20, 20)
            return (str(v) if v >= 0 else f"({v})"), F(v)
        if r < 0.8:
            a, b = rng.randint(0, 999), rng.randint(0, 999)
            s = f"{a}.{b:03d}"
            return s, F(a) + F(b, 1000)
        v = rng.randint(10**20, 10**40)
        return str(v), F(v)
    op = rng.choice("+-*/^")
    a, av = gen_num(rng, depth - 1)
    if op == "^":
        k = rng.randint(-3, 4)
        if av == 0 and k < 0:
            raise DivZero
        if abs(av.numerator) > 10**50 or av.denominator > 10**50:
            k = min(k, 2)
        return f"({a})^({k})", av ** k
    b, bv = gen_num(rng, depth - 1)
    if op == "+":
        return f"({a}+{b})", av + bv
    if op == "-":
        return f"({a}-{b})", av - bv
    if op == "*":
        return f"({a}*{b})", av * bv
    if bv == 0:
        raise DivZero
    return f"({a}/{b})", av / bv


def test_numbers(binary, rng, count):
    exprs, expected = [], []
    while len(exprs) < count:
        try:
            e, v = gen_num(rng, rng.randint(1, 5))
        except DivZero:
            continue
        exprs.append(e)
        expected.append(fstr(v))
    got = xyz(binary, exprs)
    for e, x, g in zip(exprs, expected, got):
        if x != g:
            fail("rational arithmetic", f"{e}: expected {x}, got {g}")
    if len(got) != len(exprs):
        fail("rational arithmetic", f"expected {len(exprs)} lines, got {len(got)}")


def test_division_by_zero(binary, rng, count):
    exprs = [f"{rng.randint(-50, 50)}/({rng.randint(1, 9)}-{0})*0^(-1)"
             for _ in range(count)]
    got = xyz(binary, exprs)
    for i, g in enumerate(got):
        if g != f"error: line {i + 1}: division by zero":
            fail("division by zero", g)


# ----------------------------------------------------------------- bc

def gen_int(rng, depth):
    if depth == 0 or rng.random() < 0.2:
        return str(rng.randint(0, 10**rng.randint(1, 60)))
    op = rng.choice("+-*^")
    if op == "^":
        return f"({gen_int(rng, depth - 1)})^{rng.randint(0, 5)}"
    return f"({gen_int(rng, depth - 1)}{op}{gen_int(rng, depth - 1)})"


def test_vs_bc(binary, rng, count):
    bc = shutil.which("bc")
    if not bc:
        print("skipping bc cross-check (bc not installed)")
        return
    exprs = [gen_int(rng, rng.randint(1, 4)) for _ in range(count)]
    p = subprocess.run([bc, "-q"], input="\n".join(exprs) + "\n",
                       stdout=subprocess.PIPE, text=True,
                       env={"BC_LINE_LENGTH": "0"})
    want = p.stdout.rstrip("\n").split("\n")
    got = xyz(binary, exprs)
    for e, w, g in zip(exprs, want, got):
        if w != g:
            fail("bc cross-check", f"{e}: bc {w}, xyz {g}")


# --------------------------------------------------------- polynomials

VARS = ["x", "y", "z"]


def gen_poly(rng, depth, vars_):
    if depth == 0 or rng.random() < 0.2:
        if rng.random() < 0.5:
            return rng.choice(vars_)
        v = rng.randint(-5, 5)
        if rng.random() < 0.2:
            return f"({v}/{rng.randint(1, 4)})"
        return str(v) if v >= 0 else f"({v})"
    op = rng.choice(["+", "-", "*", "*", "^"])
    if op == "^":
        return f"({gen_poly(rng, depth - 1, vars_)})^{rng.randint(0, 3)}"
    return f"({gen_poly(rng, depth - 1, vars_)}{op}{gen_poly(rng, depth - 1, vars_)})"


def to_python(s):
    s = s.replace("^", "**")
    return re.sub(r"\d+", lambda m: f"F({m.group(0)})", s)


def py_eval(s, env):
    return eval(to_python(s), {"F": F, "__builtins__": {}}, dict(env))


def rand_point(rng):
    return {v: F(rng.randint(-9, 9), rng.randint(1, 5)) for v in VARS}


def test_expand(binary, rng, count):
    polys = [gen_poly(rng, rng.randint(1, 5), VARS) for _ in range(count)]
    got = xyz(binary, [f"expand({p})" for p in polys])
    check = []
    for p, g in zip(polys, got):
        if g.startswith("error") or "(" in g.replace("(-", "").replace("/(", ""):
            # an expanded polynomial has no parenthesised sums
            if "(" in g and "/" not in g:
                fail("expand shape", f"{p} -> {g}")
                continue
        for _ in range(3):
            pt = rand_point(rng)
            try:
                a, b = py_eval(p, pt), py_eval(g, pt)
            except Exception as ex:  # noqa: BLE001
                fail("expand eval", f"{p} -> {g}: {ex}")
                break
            if a != b:
                fail("expand value", f"{p} -> {g} at {pt}: {a} != {b}")
                break
        check.append(f"expand(({p})-({g}))")
    for c, r in zip(check, xyz(binary, check)):
        if r != "0":
            fail("expand canonical", f"{c} -> {r}")


def test_eval(binary, rng, count):
    lines, want = [], []
    for _ in range(count):
        p = gen_poly(rng, rng.randint(1, 4), VARS)
        pt = rand_point(rng)
        args = ", ".join(f"{k}={v}" if v >= 0 else f"{k}=({v})"
                         for k, v in pt.items())
        lines.append(f"eval({p}, {args})")
        lines.append(f"subs({p}, {args})")
        want.append(fstr(py_eval(p, pt)))
        want.append(want[-1])
    for line, w, g in zip(lines, want, xyz(binary, lines)):
        if w != g:
            fail("eval", f"{line}: expected {w}, got {g}")


def test_diff(binary, rng, count):
    lines = []
    for _ in range(count):
        p = gen_poly(rng, rng.randint(1, 4), VARS)
        v = rng.choice(VARS)
        lines.append(f"expand(diff({p}, {v}) - diff(expand({p}), {v}))")
    for line, g in zip(lines, xyz(binary, lines)):
        if g != "0":
            fail("diff", f"{line} -> {g}")


def test_division(binary, rng, count):
    lines, cases = [], []
    while len(cases) < count:
        a = gen_poly(rng, rng.randint(1, 4), ["x", "y"])
        b = gen_poly(rng, rng.randint(1, 3), ["x"])
        if all(py_eval(b, {"x": F(t)}) == 0 for t in range(-3, 4)):
            continue  # B is the zero polynomial
        cases.append(f"A={a}; B={b};")
        lines.append(cases[-1])
        lines.append("Q=quo(A, B, x); R=rem(A, B, x);")
        lines.append("expand(Q*B+R-A)")
        lines.append("deg(R, x)")
        lines.append("deg(B, x)")
    out = xyz(binary, lines)
    if len(out) != 3 * count:
        fail("quo/rem", f"unexpected output: {out[:10]}")
        return
    for k, case in enumerate(cases):
        ident, dr, db = out[3 * k], int(out[3 * k + 1]), int(out[3 * k + 2])
        if ident != "0":
            fail("quo/rem identity", f"{case} -> {ident}")
        if not dr < db and not (db == 0 and dr == -1):
            fail("quo/rem degree", f"{case}: deg R {dr}, deg B {db}")
    zero = xyz(binary, ["quo(x^2+1, x-x, x)"])
    if zero != ["error: line 1: division by zero polynomial"]:
        fail("quo by zero", zero)


# ---------------------------------------------------------- gcd, cancel

NOT_POLY = re.compile(r"/[a-z(]")  # a division by something with variables


def test_gcd(binary, rng, count):
    """gcd(A*C, B*C) is a multiple of C and divides A*C and B*C."""
    lines, cases = [], []
    while len(cases) < count:
        a = gen_poly(rng, rng.randint(1, 3), ["x", "y"])
        b = gen_poly(rng, rng.randint(1, 3), ["x", "y"])
        c = gen_poly(rng, rng.randint(1, 3), ["x", "y"])
        pts = [{"x": F(rng.randint(-9, 9)), "y": F(rng.randint(-9, 9))} for _ in range(4)]
        if any(all(py_eval(p, pt) == 0 for pt in pts) for p in (a, b, c)):
            continue  # skip (probably) zero polynomials
        cases.append((a, b, c))
        lines.append(f"P = expand(({a})*({c})); Q = expand(({b})*({c})); "
                     f"G = gcd(P, Q);")
        lines.append("cancel(P/G)")
        lines.append("cancel(Q/G)")
        lines.append(f"cancel(G/({c}))")
    out = xyz(binary, lines)
    if len(out) != 3 * count:
        fail("gcd", f"unexpected output: {out[:6]}")
        return
    for k, (a, b, c) in enumerate(cases):
        for j, what in enumerate(("divides A*C", "divides B*C", "is a multiple of C")):
            r = out[3 * k + j]
            if r.startswith("error") or NOT_POLY.search(r):
                fail("gcd", f"gcd {what}: A={a} B={b} C={c} -> {r}")


def gen_rational(rng, depth):
    if depth == 0 or rng.random() < 0.3:
        return gen_poly(rng, 2, ["x", "y"])
    op = rng.choice("+-*/")
    return f"({gen_rational(rng, depth - 1)}){op}({gen_rational(rng, depth - 1)})"


def test_cancel(binary, rng, count):
    """cancel(R) has the same value as R wherever R is defined."""
    exprs = [gen_rational(rng, rng.randint(1, 3)) for _ in range(count)]
    got = xyz(binary, [f"cancel({e})" for e in exprs])
    for e, g in zip(exprs, got):
        if g.startswith("error"):
            # only allowed when R itself divides by zero everywhere
            if "division by zero" not in g:
                fail("cancel", f"{e} -> {g}")
            continue
        checked = 0
        for _ in range(8):
            pt = {"x": F(rng.randint(-20, 20), rng.randint(1, 3)),
                  "y": F(rng.randint(-20, 20), rng.randint(1, 3))}
            try:
                want = py_eval(e, pt)
            except ZeroDivisionError:
                continue
            try:
                have = py_eval(g, pt)
            except ZeroDivisionError:
                fail("cancel", f"{e} -> {g}: undefined at {pt}")
                break
            if want != have:
                fail("cancel", f"{e} -> {g} at {pt}: {want} != {have}")
                break
            checked += 1


# -------------------------------------------------------- complex numbers

class C:
    """Exact complex number with Fraction parts (the oracle)."""

    def __init__(self, re, im=F(0)):
        self.re, self.im = F(re), F(im)

    @staticmethod
    def of(v):
        return v if isinstance(v, C) else C(v)

    def __add__(self, o):
        o = C.of(o)
        return C(self.re + o.re, self.im + o.im)

    __radd__ = __add__

    def __neg__(self):
        return C(-self.re, -self.im)

    def __sub__(self, o):
        return self + (-C.of(o))

    def __rsub__(self, o):
        return C.of(o) - self

    def __mul__(self, o):
        o = C.of(o)
        return C(self.re * o.re - self.im * o.im, self.re * o.im + self.im * o.re)

    __rmul__ = __mul__

    def __truediv__(self, o):
        o = C.of(o)
        d = o.re * o.re + o.im * o.im
        if d == 0:
            raise ZeroDivisionError
        return self * C(o.re / d, -o.im / d)

    def __rtruediv__(self, o):
        return C.of(o) / self

    def __pow__(self, k):
        if k < 0:
            return C(1) / (self ** -k)
        r = C(1)
        for _ in range(k):
            r = r * self
        return r

    def __eq__(self, o):
        o = C.of(o)
        return self.re == o.re and self.im == o.im


def gen_complex(rng, depth):
    if depth == 0 or rng.random() < 0.3:
        r = rng.random()
        if r < 0.4:
            v = rng.randint(-9, 9)
            return f"({v})", C(v)
        if r < 0.7:
            v = rng.randint(-9, 9)
            return f"({v}*im)", C(0, v)
        a, b = rng.randint(-9, 9), rng.randint(-9, 9)
        return f"({a}+{b}*im)", C(a, b)
    op = rng.choice("+-*/^")
    a, av = gen_complex(rng, depth - 1)
    if op == "^":
        k = rng.randint(-3, 3)
        return f"({a})^({k})", av ** k
    b, bv = gen_complex(rng, depth - 1)
    if op == "+":
        return f"({a}+{b})", av + bv
    if op == "-":
        return f"({a}-{b})", av - bv
    if op == "*":
        return f"({a}*{b})", av * bv
    return f"({a}/{b})", av / bv


def parse_complex(text):
    """xyz output a, b*im, a+b*im -> C, via the oracle class."""
    return eval(to_python(text.replace("im", "IM")),
                {"F": F, "IM": C(0, 1), "__builtins__": {}})


CANONICAL = re.compile(r"^-?[0-9/]+$|^(-?[0-9/]+)?[-+]?([0-9]+\*)?im(/[0-9]+)?$")


def test_complex(binary, rng, count):
    exprs, want = [], []
    while len(exprs) < count:
        try:
            e, v = gen_complex(rng, rng.randint(1, 4))
        except ZeroDivisionError:
            continue
        exprs.append(e)
        want.append(v)
    got = xyz(binary, exprs)
    for e, w, g in zip(exprs, want, got):
        try:
            have = parse_complex(g)
        except Exception:  # noqa: BLE001
            fail("complex", f"{e}: cannot read {g}")
            continue
        if have != w:
            fail("complex", f"{e}: expected {w.re}+{w.im}*im, got {g}")
        elif not CANONICAL.match(g):
            fail("complex form", f"{e} -> {g} is not of the form a+b*im")
    lines = []
    for e in exprs:
        lines += [f"Re({e})", f"Im({e})", f"conj({e})"]
    got = xyz(binary, lines)
    for k, (e, w) in enumerate(zip(exprs, want)):
        re_, im_, cj = got[3 * k:3 * k + 3]
        try:
            ok = (parse_complex(re_) == C(w.re) and parse_complex(im_) == C(w.im)
                  and parse_complex(cj) == C(w.re, -w.im))
        except Exception:  # noqa: BLE001
            ok = False
        if not ok:
            fail("Re/Im/conj", f"{e}: {re_}, {im_}, {cj}")


def test_complex_symbols(binary, rng, count):
    """Re(e) + im*Im(e) == e and conj(conj(e)) == e with real symbols."""
    lines = []
    for _ in range(count):
        p = gen_poly(rng, rng.randint(1, 3), ["x", "y", "im"])
        lines.append(f"Re({p}) + im*Im({p}) == {p}")
        lines.append(f"conj(conj({p})) == {p}")
    for line, g in zip(lines, xyz(binary, lines)):
        if g != "1":
            fail("complex identity", f"{line} -> {g}")


# ------------------------------------------------- decimals of functions

import decimal
import functools
from decimal import Decimal as D

ORACLE_PREC = 70


@functools.lru_cache(maxsize=None)
def d_pi():
    """pi by Machin's formula, in Python's decimal module."""
    def atan_inv(n):
        s, t, k, sign = D(0), D(1) / n, 1, 1
        while t != 0:
            s += sign * t / k
            t /= n * n
            k += 2
            sign = -sign
        return s
    return 16 * atan_inv(5) - 4 * atan_inv(239)


def d_sin_cos(x):
    p = d_pi()
    n = (x / (p / 2)).to_integral_value()
    r = x - n * p / 2
    s, c, t, k = D(0), D(0), r, 1
    while abs(t) > D(10) ** (-ORACLE_PREC - 5):
        s += t
        t = -t * r * r / ((k + 1) * (k + 2))
        k += 2
    t, k = D(1), 0
    while abs(t) > D(10) ** (-ORACLE_PREC - 5):
        c += t
        t = -t * r * r / ((k + 1) * (k + 2))
        k += 2
    q = int(n) % 4
    return [(s, c), (c, -s), (-s, -c), (-c, s)][q]


def d_atan(x):
    if x < 0:
        return -d_atan(-x)
    if x > 1:
        return d_pi() / 2 - d_atan(1 / x)
    r = 0
    while x > D("0.1"):
        x = x / (1 + (1 + x * x).sqrt())
        r += 1
    s, t, k, sign = D(0), x, 1, 1
    while abs(t) > D(10) ** (-ORACLE_PREC - 5):
        s += sign * t / k
        t *= x * x
        k += 2
        sign = -sign
    return s * 2 ** r


def gen_const(rng, depth):
    """(xyz text, decimal function) for a random real constant."""
    if depth == 0 or rng.random() < 0.25:
        r = rng.random()
        if r < 0.15:
            return "pi", d_pi
        a, b = rng.randint(1, 20), rng.randint(1, 9)
        v = D(a) / D(b)
        return f"({a}/{b})", (lambda v=v: v)
    kind = rng.choice(["+", "*", "/", "sqrt", "exp", "ln", "sin", "cos",
                       "atan", "pow"])
    a, fa = gen_const(rng, depth - 1)
    if kind in "+*/":
        b, fb = gen_const(rng, depth - 1)
        ops = {"+": lambda: fa() + fb(), "*": lambda: fa() * fb(),
               "/": lambda: fa() / fb()}
        return f"({a}{kind}{b})", ops[kind]
    if kind == "sqrt":
        return f"sqrt({a}*{a})", lambda: abs(fa())
    if kind == "exp":
        return f"exp({a}/10)", lambda: (fa() / 10).exp()
    if kind == "ln":
        return f"ln(1+{a}*{a})", lambda: (1 + fa() * fa()).ln()
    if kind == "sin":
        return f"sin({a})", lambda: d_sin_cos(fa())[0]
    if kind == "cos":
        return f"cos({a})", lambda: d_sin_cos(fa())[1]
    if kind == "atan":
        return f"atan({a})", lambda: d_atan(fa())
    k = rng.randint(2, 5)
    return f"(1+{a}*{a})^(1/{k})", lambda: (1 + fa() * fa()) ** (D(1) / k)


def test_num(binary, rng, count):
    decimal.getcontext().prec = ORACLE_PREC
    exprs, want = [], []
    while len(exprs) < count:
        e, fe = gen_const(rng, rng.randint(1, 4))
        try:
            v = fe()
        except (decimal.DivisionByZero, decimal.InvalidOperation, ZeroDivisionError):
            continue
        if abs(v) > D(10) ** 30:
            continue
        exprs.append(e)
        want.append(v)
    got = xyz(binary, [f"num({e}, 30)" for e in exprs])
    for e, w, g in zip(exprs, want, got):
        try:
            have = D(g)
        except decimal.InvalidOperation:
            fail("num", f"{e}: {g}")
            continue
        if abs(have - w) > D(10) ** -30 * D("0.6"):  # half a unit, and a bit
            fail("num", f"{e}: expected {w:.32f}, got {g}")


def gen_fun(rng, depth, in_exp=False):
    """a random function of x built from the elementary functions
    (no exp inside exp: exp(exp(exp(x))) has thousands of digits)"""
    if depth == 0 or rng.random() < 0.25:
        return rng.choice(["x", "x^2", f"({rng.randint(1, 5)}*x)", "(x+1)"])
    choices = ["sin", "cos", "atan", "ln", "*", "+", "/"] + ([] if in_exp else ["exp"])
    f = rng.choice(choices)
    a = gen_fun(rng, depth - 1, in_exp or f == "exp")
    if f in "*+/":
        b = gen_fun(rng, depth - 1, in_exp)
        return f"({a}{f}{b})"
    if f == "ln":
        return f"ln(1+({a})^2)"
    return f"{f}({a})"


def test_diff_numeric(binary, rng, count):
    """diff(f) at a point agrees with a central difference."""
    # The central difference is off by about h^2 times the third
    # derivative / 6: with h = 10^-20 that is far below the tolerance, and
    # 40 digits (+ 20 guard digits) cover the cancellation in f(a+h)-f(a-h).
    h = "1/10^20"
    lines = []
    for _ in range(count):
        f = gen_fun(rng, rng.randint(1, 3))
        a = f"{rng.randint(1, 9)}/{rng.randint(2, 9)}"
        # eval: diff gives an expression in x, then x is set
        lines.append(f"num(eval(diff({f}, x), x={a}), 40)")
        lines.append(f"num(eval(diff({f}, x), x={a}) - (subs({f}, x={a}+{h}) - "
                     f"subs({f}, x={a}-{h}))/(2*{h}), 40)")
    out = xyz(binary, lines)
    for k in range(count):
        deriv, delta = out[2 * k], out[2 * k + 1]
        try:
            ok = abs(D(delta)) <= D(10) ** -15 * max(D(1), abs(D(deriv)))
        except decimal.InvalidOperation:
            ok = "division by zero" in delta
        if not ok:
            fail("diff (numeric)", f"{lines[2 * k + 1]} -> {delta}")


def test_trigexpand(binary, rng, count):
    """trigexpand keeps the value."""
    lines = []
    for _ in range(count):
        f = rng.choice(["sin", "cos", "tan"])
        n = rng.randint(2, 7)
        u = f"{rng.randint(1, 9)}/{rng.randint(2, 9)}"
        v = f"{rng.randint(1, 9)}/{rng.randint(2, 9)}"
        arg = rng.choice([f"{n}*y", f"y+z", f"{n}*y+z"])
        lines.append(f"num(subs(trigexpand({f}({arg})) - {f}({arg}), y={u}, z={v}), 30)")
    for line, g in zip(lines, xyz(binary, lines)):
        try:
            ok = abs(D(g)) <= D(10) ** -29
        except decimal.InvalidOperation:
            ok = False
        if not ok:
            fail("trigexpand", f"{line} -> {g}")


# ---------------------------------------------- variables: '=' and ':='

def test_links(binary, rng, count):
    """Random mix of  v = expr  (snapshot) and  v := expr  (formula),
    checked against a Python model of the two semantics."""
    names = ["a", "b", "c", "d", "e"]
    defs = {}  # name -> ("v", Fraction) or ("f", formula text)

    def deps(expr):
        return set(re.findall(r"[a-e]", expr))

    def depends(name, target):
        if name == target:
            return True
        kind, d = defs[name]
        return kind == "f" and any(depends(n, target) for n in deps(d))

    def value(expr, cache):
        env = {}
        for n in deps(expr):
            if n not in cache:
                kind, d = defs[n]
                cache[n] = d if kind == "v" else value(d, cache)
            env[n] = cache[n]
        return py_eval(expr, env)

    def text(name):
        kind, d = defs[name]
        return f"({d})"

    def inline(expr, target):
        def rep(m):
            n = m.group(0)
            if n == target:
                return text(n)
            if defs[n][0] == "f" and depends(n, target):
                return f"({inline(defs[n][1], target)})"
            return n
        return re.sub(r"[a-e]", rep, expr)

    def small(n):
        v = rng.randint(1, 9)
        defs[n] = ("v", F(v))
        return f"{n} = {v};"

    lines, want = [], []
    for n in names:
        lines.append(small(n))
    for _ in range(count):
        n, k, m = rng.choice(names), rng.choice(names), rng.choice(names)
        snapshot = rng.random() < 0.5
        # Only + and * on positive values: then no variable can cancel out
        # (xyz tidies a-a or d*0 away when storing a formula, which this
        # text-based model would not see).
        expr = f"{k}{rng.choice('+*')}{m}+{rng.randint(0, 5)}"
        if snapshot:
            defs[n] = ("v", value(expr, {}))
            lines.append(f"{n} = {expr};")
        else:
            # self references are replaced by the old definition, like xyz
            if any(depends(x, n) for x in deps(expr)):
                expr2 = inline(expr, n)
            else:
                expr2 = expr
            defs[n] = ("f", expr2)
            lines.append(f"{n} := {expr};")
        v = value(f"({n})", {})
        if len(defs[n][1] if defs[n][0] == "f" else "") > 300 or \
                max(abs(v.numerator), v.denominator).bit_length() > 2000:
            lines.append(small(n))  # keep the model fast and numbers small
        q = rng.choice(names)
        v = value(f"({q})", {})
        if max(abs(v.numerator), v.denominator).bit_length() > 4000:
            lines.append(small(q))
            v = value(f"({q})", {})
        lines.append(q)
        want.append(fstr(v))
    got = xyz(binary, lines)
    if len(got) != len(want):
        fail("variables", f"expected {len(want)} lines, got {len(got)}: {got[:5]}")
        return
    for w, g in zip(want, got):
        if w != g:
            fail("variables", f"expected {w}, got {g}")
            break


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary", nargs="?", default="./xyz")
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--seed", type=int, default=None)
    a = ap.parse_args()
    seed = a.seed if a.seed is not None else random.randrange(1 << 30)
    rng = random.Random(seed)
    print(f"random tests, seed {seed} (rerun with --seed {seed})")
    tests = [test_numbers, test_division_by_zero, test_vs_bc, test_expand,
             test_eval, test_diff, test_division, test_gcd, test_cancel,
             test_complex, test_complex_symbols, test_num, test_diff_numeric,
             test_trigexpand, test_links]
    for t in tests:
        before = failures
        t(a.binary, rng, a.count)
        print(f"  {t.__name__:24s} {'ok' if failures == before else 'FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
