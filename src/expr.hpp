// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Expression trees. Nodes are immutable and shared via shared_ptr.
#pragma once

#include "rational.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xyz {

enum class Kind {
	Num,  // exact rational
	Sym,  // symbol / variable
	Add,  // n-ary sum
	Mul,  // n-ary product
	Pow,  // args[0] ^ args[1]
	Call, // name(args...)
	Arg,  // name = args[0]  (eval/table argument), or a range
	      // name = args[0] to args[1] step args[2]  (table argument)
	Dec,  // decimal text produced by num(); cannot be computed with
	Str,  // "text": only as a file name for save() and transcript()
};

struct Node;
using Expr = std::shared_ptr<const Node>;
using Exprs = std::vector<Expr>;

struct Node {
	Kind kind;
	Rational num;     // Num
	std::string name; // Sym, Call, Arg, Dec (the text)
	Exprs args;
};

// --- construction ---------------------------------------------------
Expr mkNum(Rational r);
Expr mkInt(long v);
Expr mkSym(std::string name);
Expr mkDec(std::string text);
Expr mkStr(std::string text);
Expr mkNode(Kind k, Exprs args);
Expr mkPow(Expr b, Expr e);
Expr mkCall(std::string name, Exprs args);
Expr mkArg(std::string name, Expr value);
Expr mkRange(std::string name, Expr start, Expr stop, Expr step);
inline bool isRange(const Expr& e) { return e->kind == Kind::Arg && e->args.size() == 3; }

// raw (unsimplified) builders, used by the parser
Expr rawAdd(Expr a, Expr b);
Expr rawMul(Expr a, Expr b);
Expr rawSub(Expr a, Expr b);
Expr rawDiv(Expr a, Expr b);
Expr rawNeg(Expr a);

// --- inspection -----------------------------------------------------
int compare(const Expr& a, const Expr& b); // total structural order
inline bool equal(const Expr& a, const Expr& b) { return compare(a, b) == 0; }
bool isInt(const Expr& e, long& v);
bool isNumber(const Expr& e, long v);
bool isZero(const Expr& e);
bool containsSym(const Expr& e, const std::string& name);

// --- algebra (simplify.cpp) ---------------------------------------------
// Inputs must already be simplified; results are simplified.
Expr simpAdd(const Exprs& terms);
Expr simpMul(const Exprs& factors);
Expr simpPow(const Expr& base, const Expr& exp);
Expr simplify(const Expr& raw); // raw tree without calls
// Tidies a stored formula without changing where it is defined: like
// terms and equal powers are combined, but nothing that removes a
// division (x/x stays x/x). Calls are kept, with their arguments tidied.
Expr safeSimplify(const Expr& raw);
Expr add(const Expr& a, const Expr& b);
Expr mul(const Expr& a, const Expr& b);
Expr sub(const Expr& a, const Expr& b);
Expr expand(const Expr& e);
Expr diff(const Expr& e, const std::string& x);
long polyDegree(const Expr& e, const std::string& x); // -1 for zero
// degree -> coefficient of x^degree (zero coefficients left out)
std::map<long, Expr> polyCoefficients(const Expr& e, const std::string& x);
// does e divide by something containing a symbol?
bool hasVariableDenominator(const Expr& e);
Expr polyCoeff(const Expr& e, const std::string& x, long k);
std::pair<Expr, Expr> polyDivide(const Expr& p, const Expr& q, const std::string& x);

// --- complex numbers --------------------------------------------------
// The imaginary unit is the symbol "%i" inside (it cannot be typed). On
// input and output it is called imagName(): "im" unless --imag=NAME.
extern const char* const kImag;
inline bool isImUnit(const Expr& e) { return e->kind == Kind::Sym && e->name == kImag; }
inline Expr mkIm() { return mkSym(kImag); }
const std::string& imagName();
void setImagName(const std::string& name);
// Exact complex number a+b*im (also plain numbers)?
bool complexParts(const Expr& e, Rational& re, Rational& im);
Expr makeComplex(const Rational& re, const Rational& im);
// Calls that may stay in results when they cannot be evaluated further,
// e.g. Re(x^(1/2)).
bool isSymbolicFunction(const std::string& name);
// complex.cpp: real and imaginary parts where they can be found, with
// symbols taken to be real; otherwise the call itself, e.g. Re(x^(1/2)).
Expr realPart(const Expr& e);
Expr imagPart(const Expr& e);
Expr conjugate(const Expr& e);
Expr absValue(const Expr& e);

// --- elementary functions (elem.cpp) -----------------------------------
// sin cos tan asin acos atan exp ln
bool isElementary(const std::string& name);
// f(args) with the exact values and rules applied (also log(b, x))
Expr makeFunction(const std::string& name, const Exprs& args);
Expr derivativeOf(const std::string& name, const Expr& u); // f'(u)
Expr trigexpand(const Expr& e); // addition and multiple-angle formulas

// --- decimals (numeric.cpp) ------------------------------------------
// num(): numbers (also sqrt(2), pi, sin(1), ...) as decimals with the given
// number of decimals; inside expressions with symbols, the numbers are
// converted and the symbols kept.
Expr numeric(const Expr& e, size_t digits);
void setNumTruncate(bool truncate); // --trunc: truncate like bc, not round
void clearNumericCaches();          // before bcl shuts down

// --- gcd and cancellation (gcd.cpp) ----------------------------------
Rational gcd(const Rational& a, const Rational& b); // >= 0; gcd(a/b, c/d) = gcd(a,c)/lcm(b,d)
Rational lcm(const Rational& a, const Rational& b); // >= 0
// GCD / LCM of polynomials with rational coefficients in any number of
// variables, normalised to integer coefficients without common factor and
// a positive leading coefficient. Errors for non-polynomials.
Expr polyGcd(const Expr& a, const Expr& b);
Expr polyLcm(const Expr& a, const Expr& b);
// Writes e as one fraction p/q of polynomials with common factors
// cancelled. Non-polynomial parts (roots, symbolic powers) are kept as
// they are and treated like symbols.
Expr cancel(const Expr& e);
// Is e identically zero (after bringing it to one fraction)?
bool isIdenticallyZero(const Expr& e);

// --- printing (print.cpp) -------------------------------------------
std::string toString(const Expr& e);
std::string toLatex(const Expr& e);        // LaTeX math, e.g. \frac{x+1}{2}
std::string latexName(const std::string& name); // x, \alpha, \mathrm{abc}, x_{1}
std::string latexText(const std::string& text); // escapes \ { } $ & # ^ _ % ~

} // namespace xyz
