// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Elementary functions: sin, cos, tan, asin, acos, atan, exp, ln, log, and
// the constant pi. Kept basic:
//  - exact values at multiples of pi/6 and pi/4 (and the inverse values)
//  - symmetries: sin(-x) = -sin(x), cos(x + k*pi) = (-1)^k cos(x), ...
//  - exp(0) = 1, ln(1) = 0, exp(ln(x)) = x, ln(exp(x)) = x (x real),
//    exp(k*pi*im) for the standard angles, ln of negative numbers
//  - sin(asin(x)) = x, cos(acos(x)) = x, tan(atan(x)) = x
// Anything else stays as it is: sin(1), ln(x).
#include "expr.hpp"

#include <algorithm>
#include <map>
#include <tuple>
#include <optional>

namespace xyz {

namespace {

bool isPi(const Expr& e) { return e->kind == Kind::Sym && e->name == "pi"; }
Expr piSym() { return mkSym("pi"); }
Expr half(const Expr& e) { return mul(mkNum(Rational(1) / Rational(2)), e); }
Expr sqrtOf(long n) { return simpPow(mkInt(n), mkNum(Rational(1) / Rational(2))); }
Expr neg(const Expr& e) { return mul(mkInt(-1), e); }

// u = k*pi with rational k?
bool piMultiple(const Expr& u, Rational& k)
{
	if (u->kind == Kind::Num && u->num.isZero()) {
		k = Rational(0);
		return true;
	}
	if (isPi(u)) {
		k = Rational(1);
		return true;
	}
	if (u->kind == Kind::Mul && u->args.size() == 2 && u->args[0]->kind == Kind::Num &&
	    isPi(u->args[1])) {
		k = u->args[0]->num;
		return true;
	}
	return false;
}

// u = rest + k*pi: finds a pi term in a sum
bool piShift(const Expr& u, Rational& k, Expr& rest)
{
	if (u->kind != Kind::Add)
		return false;
	for (size_t i = 0; i < u->args.size(); i++) {
		if (piMultiple(u->args[i], k) && !k.isZero()) {
			Exprs others;
			for (size_t j = 0; j < u->args.size(); j++)
				if (j != i)
					others.push_back(u->args[j]);
			rest = simpAdd(others);
			return true;
		}
	}
	return false;
}

// Does u look negative (-x, -3, -x+1)? Then f(u) is written with f(-u).
bool looksNegative(const Expr& u)
{
	switch (u->kind) {
	case Kind::Num:
		return u->num.sign() < 0;
	case Kind::Mul:
		return u->args[0]->kind == Kind::Num && u->args[0]->num.sign() < 0;
	case Kind::Add:
		return looksNegative(u->args[0]);
	default:
		return false;
	}
}

Rational floorOf(const Rational& r)
{
	BigInt q = r.numerator().divTrunc(r.denominator());
	if (r.sign() < 0 && !r.isInt())
		q = q - BigInt(1L);
	return Rational(q, BigInt(1L));
}

// sin(k*pi) for the standard angles
std::optional<Expr> sinPi(Rational k)
{
	k = k - Rational(2) * floorOf(k / Rational(2)); // 0 <= k < 2
	if (k.compare(Rational(1)) > 0) {
		auto v = sinPi(k - Rational(1));
		if (v)
			return neg(*v);
		return std::nullopt;
	}
	if (k.compare(Rational(1) / Rational(2)) > 0)
		k = Rational(1) - k;
	// 0 <= k <= 1/2
	auto is = [&k](long p, long q) { return k.compare(Rational(p) / Rational(q)) == 0; };
	if (k.isZero())
		return mkInt(0);
	if (is(1, 6))
		return mkNum(Rational(1) / Rational(2));
	if (is(1, 4))
		return half(sqrtOf(2));
	if (is(1, 3))
		return half(sqrtOf(3));
	if (is(1, 2))
		return mkInt(1);
	return std::nullopt;
}

std::optional<Expr> cosPi(const Rational& k) { return sinPi(k + Rational(1) / Rational(2)); }

// the angle in [-pi/2, pi/2] whose sine is v, for the standard values
std::optional<Expr> asinValue(const Expr& v)
{
	const long qs[][2] = {{0, 1}, {1, 6}, {1, 4}, {1, 3}, {1, 2}};
	for (const auto& q : qs) {
		Rational k = Rational(q[0]) / Rational(q[1]);
		Expr s = *sinPi(k);
		if (equal(v, s))
			return mul(mkNum(k), piSym());
		if (equal(v, neg(s)))
			return mul(mkNum(-k), piSym());
	}
	return std::nullopt;
}

// the angle in (-pi/2, pi/2) whose tangent is v, for the standard values
std::optional<Expr> atanValue(const Expr& v)
{
	const long qs[][2] = {{0, 1}, {1, 6}, {1, 4}, {1, 3}};
	for (const auto& q : qs) {
		Rational k = Rational(q[0]) / Rational(q[1]);
		Expr t = mul(*sinPi(k), simpPow(*cosPi(k), mkInt(-1)));
		if (equal(v, t))
			return mul(mkNum(k), piSym());
		if (equal(v, neg(t)))
			return mul(mkNum(-k), piSym());
	}
	return std::nullopt;
}

bool isCall(const Expr& e, const char* name) { return e->kind == Kind::Call && e->name == name; }

Expr fsin(const Expr& u);
Expr fcos(const Expr& u);

// u = m*pi with m an integer by assumption (n*pi, 2*n*pi)
bool intPiMultiple(const Expr& u, Expr& m)
{
	if (u->kind != Kind::Mul)
		return false;
	Exprs rest;
	bool hasPi = false;
	for (const Expr& f : u->args) {
		if (isPi(f) && !hasPi)
			hasPi = true;
		else
			rest.push_back(f);
	}
	if (!hasPi)
		return false;
	m = simpMul(rest);
	return m->kind != Kind::Num && isIntegerValued(m);
}

// u = rest + m*pi with integer m (symbolic): finds such a term
bool intPiShift(const Expr& u, Expr& m, Expr& rest)
{
	if (u->kind != Kind::Add)
		return false;
	for (size_t i = 0; i < u->args.size(); i++)
		if (intPiMultiple(u->args[i], m)) {
			Exprs others;
			for (size_t j = 0; j < u->args.size(); j++)
				if (j != i)
					others.push_back(u->args[j]);
			rest = simpAdd(others);
			return true;
		}
	return false;
}

Expr fsin(const Expr& u)
{
	Rational k;
	Expr rest, m;
	if (intPiMultiple(u, m)) // sin(n*pi) = 0
		return mkInt(0);
	if (intPiShift(u, m, rest)) // sin(x + n*pi) = (-1)^n sin(x)
		return mul(simpPow(mkInt(-1), m), fsin(rest));
	if (piMultiple(u, k)) {
		if (auto v = sinPi(k))
			return *v;
	} else if (piShift(u, k, rest) && k.isInt()) { // sin(x + k*pi) = (-1)^k sin(x)
		Expr s = fsin(rest);
		return floorOf(k / Rational(2)).compare(k / Rational(2)) == 0 ? s : neg(s);
	}
	if (looksNegative(u))
		return neg(fsin(neg(u)));
	if (isCall(u, "asin"))
		return u->args[0];
	return mkCall("sin", {u});
}

Expr fcos(const Expr& u)
{
	Rational k;
	Expr rest, m;
	if (intPiMultiple(u, m)) // cos(n*pi) = (-1)^n
		return simpPow(mkInt(-1), m);
	if (intPiShift(u, m, rest))
		return mul(simpPow(mkInt(-1), m), fcos(rest));
	if (piMultiple(u, k)) {
		if (auto v = cosPi(k))
			return *v;
	} else if (piShift(u, k, rest) && k.isInt()) {
		Expr c = fcos(rest);
		return floorOf(k / Rational(2)).compare(k / Rational(2)) == 0 ? c : neg(c);
	}
	if (looksNegative(u))
		return fcos(neg(u));
	if (isCall(u, "acos"))
		return u->args[0];
	return mkCall("cos", {u});
}

Expr ftan(const Expr& u)
{
	Rational k;
	Expr rest;
	if (piMultiple(u, k)) {
		auto s = sinPi(k), c = cosPi(k);
		if (s && c) {
			if (isZero(*c))
				throw Error("tan(" + toString(u) + ") is undefined");
			return mul(*s, simpPow(*c, mkInt(-1)));
		}
	} else if (piShift(u, k, rest) && k.isInt()) { // period pi
		return ftan(rest);
	}
	if (looksNegative(u))
		return neg(ftan(neg(u)));
	if (isCall(u, "atan"))
		return u->args[0];
	return mkCall("tan", {u});
}

Expr fasin(const Expr& v)
{
	if (auto a = asinValue(v))
		return *a;
	if (looksNegative(v))
		return neg(fasin(neg(v)));
	return mkCall("asin", {v});
}

Expr facos(const Expr& v)
{
	if (auto a = asinValue(v)) // acos(v) = pi/2 - asin(v)
		return sub(half(piSym()), *a);
	if (looksNegative(v)) // acos(-v) = pi - acos(v)
		return sub(piSym(), facos(neg(v)));
	return mkCall("acos", {v});
}

Expr fatan(const Expr& v)
{
	if (auto a = atanValue(v))
		return *a;
	if (looksNegative(v))
		return neg(fatan(neg(v)));
	return mkCall("atan", {v});
}

Expr fexp(const Expr& u)
{
	if (isZero(u))
		return mkInt(1);
	if (isCall(u, "ln"))
		return u->args[0];
	// exp(k*pi*im) for the standard angles: exp(pi*im) = -1
	if (u->kind == Kind::Mul) {
		Exprs rest;
		bool hasIm = false;
		for (const Expr& f : u->args) {
			if (isImUnit(f) && !hasIm)
				hasIm = true;
			else
				rest.push_back(f);
		}
		Rational k;
		if (hasIm && piMultiple(simpMul(rest), k)) {
			auto c = cosPi(k), s = sinPi(k);
			if (c && s)
				return add(*c, mul(mkIm(), *s));
		}
	}
	return mkCall("exp", {u});
}

Expr fln(const Expr& v)
{
	if (v->kind == Kind::Num) {
		if (v->num.isZero())
			throw Error("ln(0) is undefined");
		if (v->num.isOne())
			return mkInt(0);
		if (v->num.sign() < 0) // principal value: ln(-x) = ln(x) + pi*im
			return add(fln(mkNum(-v->num)), mul(piSym(), mkIm()));
		if (v->num.numerator().isOne()) // ln(1/q) = -ln(q)
			return neg(fln(mkNum(Rational(v->num.denominator(), BigInt(1L)))));
	}
	// ln(exp(x)) = x for real x (symbols are real)
	if (isCall(v, "exp") && !containsSym(v->args[0], kImag))
		return v->args[0];
	// with assumptions: ln(a*b) = ln(a) + ln(b) for a, b > 0, ln(a^r) = r*ln(a)
	if (v->kind == Kind::Mul) {
		bool all = true;
		for (const Expr& f : v->args)
			all = all && positive(f);
		if (all) {
			Exprs t;
			for (const Expr& f : v->args)
				t.push_back(fln(f));
			return simpAdd(t);
		}
	}
	if (v->kind == Kind::Pow && positive(v->args[0]) && !containsSym(v->args[1], kImag) &&
	    v->args[0]->kind != Kind::Num)
		return mul(v->args[1], fln(v->args[0]));
	return mkCall("ln", {v});
}

// log(b, x) = ln(x)/ln(b); exact when x is a power of b: log(2, 8) = 3
Expr flog(const Expr& b, const Expr& x)
{
	if (b->kind == Kind::Num && x->kind == Kind::Num && b->num.sign() > 0 &&
	    !b->num.isOne() && x->num.sign() > 0) {
		if (x->num.isOne())
			return mkInt(0);
		Rational p = b->num, q = x->num;
		for (long k = 1; k <= 64; k++, p = p * b->num, q = q * x->num) {
			if (p.compare(x->num) == 0) // b^k = x
				return mkInt(k);
			if (p.compare(Rational(1) / x->num) == 0)
				return mkInt(-k);
			if (q.compare(b->num) == 0) // x^k = b
				return mkNum(Rational(1) / Rational(k));
			if (q.compare(Rational(1) / b->num) == 0)
				return mkNum(Rational(-1) / Rational(k));
		}
	}
	if (b->kind == Kind::Num && (b->num.sign() <= 0 || b->num.isOne()))
		throw Error("log: the base must be positive and not 1");
	return mul(fln(x), simpPow(fln(b), mkInt(-1)));
}

// hyperbolic functions: values at 0, symmetries, inverses
Expr fhyp(const std::string& name, const Expr& u)
{
	bool odd = name != "cosh";
	if (isZero(u))
		return name == "cosh" ? mkInt(1) : mkInt(0);
	if (looksNegative(u))
		return odd ? neg(fhyp(name, neg(u))) : fhyp(name, neg(u));
	if (isCall(u, ("a" + name).c_str()))
		return u->args[0]; // sinh(asinh(x)) = x
	return mkCall(name, {u});
}

Expr fahyp(const std::string& name, const Expr& v)
{
	if (name == "acosh") {
		if (isNumber(v, 1))
			return mkInt(0);
		return mkCall(name, {v});
	}
	if (isZero(v))
		return mkInt(0);
	if (looksNegative(v)) // asinh and atanh are odd
		return neg(fahyp(name, neg(v)));
	if (name == "asinh" && isCall(v, "sinh"))
		return v->args[0];
	if (name == "atanh" && isCall(v, "tanh"))
		return v->args[0];
	if (name == "atanh" && (isNumber(v, 1)))
		throw Error("atanh(1) is undefined");
	return mkCall(name, {v});
}

} // namespace

bool isElementary(const std::string& n)
{
	return n == "sin" || n == "cos" || n == "tan" || n == "asin" || n == "acos" ||
	       n == "atan" || n == "exp" || n == "ln" || n == "sinh" || n == "cosh" ||
	       n == "tanh" || n == "asinh" || n == "acosh" || n == "atanh";
}

Expr makeFunction(const std::string& name, const Exprs& a)
{
	if (name == "sin") return fsin(a[0]);
	if (name == "cos") return fcos(a[0]);
	if (name == "tan") return ftan(a[0]);
	if (name == "asin") return fasin(a[0]);
	if (name == "acos") return facos(a[0]);
	if (name == "atan") return fatan(a[0]);
	if (name == "exp") return fexp(a[0]);
	if (name == "ln") return fln(a[0]);
	if (name == "sinh" || name == "cosh" || name == "tanh") return fhyp(name, a[0]);
	if (name == "asinh" || name == "acosh" || name == "atanh") return fahyp(name, a[0]);
	if (name == "log")
		return a.size() == 1 ? fln(a[0]) : flog(a[0], a[1]);
	throw Error("unknown function '" + name + "'");
}

// derivative of f(u) with respect to u (the chain rule is applied by diff)
Expr derivativeOf(const std::string& name, const Expr& u)
{
	Expr minusHalf = mkNum(Rational(-1) / Rational(2));
	if (name == "sin") return fcos(u);
	if (name == "cos") return neg(fsin(u));
	if (name == "tan") return simpPow(fcos(u), mkInt(-2));
	if (name == "exp") return fexp(u);
	if (name == "ln") return simpPow(u, mkInt(-1));
	if (name == "asin") return simpPow(sub(mkInt(1), mul(u, u)), minusHalf);
	if (name == "acos") return neg(simpPow(sub(mkInt(1), mul(u, u)), minusHalf));
	if (name == "atan") return simpPow(add(mkInt(1), mul(u, u)), mkInt(-1));
	if (name == "sinh") return fhyp("cosh", u);
	if (name == "cosh") return fhyp("sinh", u);
	if (name == "tanh") return simpPow(fhyp("cosh", u), mkInt(-2));
	if (name == "asinh") return simpPow(add(mul(u, u), mkInt(1)), minusHalf);
	if (name == "acosh") return simpPow(sub(mul(u, u), mkInt(1)), minusHalf);
	if (name == "atanh") return simpPow(sub(mkInt(1), mul(u, u)), mkInt(-1));
	throw Error("diff: cannot differentiate " + name + "()");
}

// ------------------------------------------------------------ trigexpand

namespace {

Expr trigexpandRec(const Expr& e);

// x with the symbols ~c, ~s, ~t replaced by c, s, t
Expr putTrig(const Expr& x, const Expr& c, const Expr& s, const Expr& t)
{
	if (x->kind == Kind::Sym)
		return x->name == "~c" ? c : x->name == "~s" ? s : x->name == "~t" ? t : x;
	if (x->args.empty())
		return x;
	auto m = std::make_shared<Node>(*x);
	for (Expr& a : m->args)
		a = putTrig(a, c, s, t);
	return simplify(m);
}

// sin(n*u) and cos(n*u) from (cos(u) + im*sin(u))^n
std::pair<Expr, Expr> multipleAngle(long n, const Expr& u)
{
	Expr c = mkSym("~c"), s = mkSym("~s");
	Expr z = expand(simpPow(add(c, mul(mkIm(), s)), mkInt(n)));
	Expr re = realPart(z), im = imagPart(z);
	Expr cu = fcos(u), su = fsin(u);
	auto put = [&](const Expr& x) { return expand(putTrig(x, cu, su, x)); };
	return {put(im), put(re)}; // sin, cos
}

// e as one fraction p/q, with the signs chosen so that q has a positive
// constant term where it has one: (tan(x)+tan(y))/(1-tan(x)*tan(y))
Expr tanQuotient(const Expr& e)
{
	auto [p, q] = cancelParts(e);
	Exprs qt = q->kind == Kind::Add ? q->args : Exprs{q};
	for (const Expr& t : qt)
		if (t->kind == Kind::Num && t->num.sign() < 0) {
			p = expand(neg(p));
			q = expand(neg(q));
			break;
		}
	return mul(p, simpPow(q, mkInt(-1)));
}

// tan(n*u) = Im/Re of (1 + im*tan(u))^n, which is cos(u)^-n (cos(nu) + im*sin(nu))
Expr multipleAngleTan(long n, const Expr& u)
{
	Expr t = mkSym("~t");
	Expr z = expand(simpPow(add(mkInt(1), mul(mkIm(), t)), mkInt(n)));
	return tanQuotient(putTrig(mul(imagPart(z), simpPow(realPart(z), mkInt(-1))), t, t, ftan(u)));
}

Expr expandSin(const Expr& u);
Expr expandCos(const Expr& u);

// u = a + b: the first term and the rest
bool splitSum(const Expr& u, Expr& a, Expr& b)
{
	if (u->kind != Kind::Add)
		return false;
	a = u->args[0];
	b = simpAdd(Exprs(u->args.begin() + 1, u->args.end()));
	return true;
}

bool integerMultiple(const Expr& u, long& n, Expr& v)
{
	if (u->kind == Kind::Mul && u->args[0]->kind == Kind::Num &&
	    u->args[0]->num.toLong(n) && n > 1 && n <= 1000) {
		Exprs rest(u->args.begin() + 1, u->args.end());
		v = rest.size() == 1 ? rest[0] : mkNode(Kind::Mul, rest);
		return true;
	}
	return false;
}

Expr expandSin(const Expr& u)
{
	Expr a, b, v;
	long n;
	if (splitSum(u, a, b)) // sin(a+b) = sin(a)cos(b) + cos(a)sin(b)
		return expand(add(mul(expandSin(a), expandCos(b)), mul(expandCos(a), expandSin(b))));
	if (integerMultiple(u, n, v))
		return multipleAngle(n, v).first;
	return fsin(u);
}

Expr expandCos(const Expr& u)
{
	Expr a, b, v;
	long n;
	if (splitSum(u, a, b)) // cos(a+b) = cos(a)cos(b) - sin(a)sin(b)
		return expand(sub(mul(expandCos(a), expandCos(b)), mul(expandSin(a), expandSin(b))));
	if (integerMultiple(u, n, v))
		return multipleAngle(n, v).second;
	return fcos(u);
}

// tan(a+b) = (tan(a) + tan(b))/(1 - tan(a)*tan(b))
Expr expandTan(const Expr& u)
{
	Expr a, b, v;
	long n;
	if (splitSum(u, a, b)) {
		// b = pi/2 + k*pi: tan(a+b) = -1/tan(a)
		if (isZero(fcos(b)))
			return neg(simpPow(expandTan(a), mkInt(-1)));
		if (isZero(fcos(a)))
			return neg(simpPow(expandTan(b), mkInt(-1)));
		Expr ta = expandTan(a), tb = expandTan(b);
		return tanQuotient(mul(add(ta, tb), simpPow(sub(mkInt(1), mul(ta, tb)), mkInt(-1))));
	}
	if (integerMultiple(u, n, v))
		return multipleAngleTan(n, v);
	return ftan(u);
}

Expr trigexpandRec(const Expr& e)
{
	switch (e->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(trigexpandRec(x));
		return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(trigexpandRec(e->args[0]), e->args[1]);
	case Kind::Call: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(trigexpandRec(x));
		if (e->name == "sin")
			return expandSin(a[0]);
		if (e->name == "cos")
			return expandCos(a[0]);
		if (e->name == "tan")
			return expandTan(a[0]);
		return isElementary(e->name) ? makeFunction(e->name, a) : mkCall(e->name, a);
	}
	default:
		return e;
	}
}

} // namespace

Expr trigexpand(const Expr& e) { return trigexpandRec(e); }

// ------------------------------------------------------------ trigreduce
//
// The opposite of trigexpand: products and powers of sin and cos are
// written as sums of sin and cos of combined angles, with
//   sin a sin b = (cos(a-b) - cos(a+b))/2
//   cos a cos b = (cos(a-b) + cos(a+b))/2
//   sin a cos b = (sin(a+b) + sin(a-b))/2
// so that 2*sin(x)*cos(x) -> sin(2*x) and cos(x)^2 -> cos(2*x)/2 + 1/2.
// tan and negative powers (1/cos(x)) are left as they are.

namespace {

// A sum of c*sin(angle) and c*cos(angle), keyed by (isSin, angle)
struct TrigKey {
	bool isSin;
	Expr angle;
	bool operator<(const TrigKey& o) const
	{
		if (isSin != o.isSin)
			return isSin < o.isSin;
		return compare(angle, o.angle) < 0;
	}
};
using TrigSum = std::map<TrigKey, Expr>;

// adds c*f(angle) to the sum
void addTrigTerm(TrigSum& sum, bool isSin, Expr angle, Expr c)
{
	if (looksNegative(angle)) { // sin(-a) = -sin(a), cos(-a) = cos(a)
		angle = neg(angle);
		if (isSin)
			c = neg(c);
	}
	if (isSin && isZero(angle))
		return;
	auto [it, inserted] = sum.try_emplace(TrigKey{isSin, angle}, c);
	if (!inserted)
		it->second = add(it->second, c);
}

// product of two sums, with the product-to-sum formulas
TrigSum mulTrig(const TrigSum& p, const TrigSum& q)
{
	TrigSum r;
	for (const auto& [ka, ca] : p)
		for (const auto& [kb, cb] : q) {
			Expr c = half(mul(ca, cb));
			const Expr &a = ka.angle, &b = kb.angle;
			Expr plus = add(a, b), minus = sub(a, b);
			if (ka.isSin && kb.isSin) {
				addTrigTerm(r, false, minus, c);
				addTrigTerm(r, false, plus, neg(c));
			} else if (!ka.isSin && !kb.isSin) {
				addTrigTerm(r, false, minus, c);
				addTrigTerm(r, false, plus, c);
			} else if (ka.isSin) { // sin a cos b
				addTrigTerm(r, true, plus, c);
				addTrigTerm(r, true, minus, c);
			} else { // cos a sin b = (sin(a+b) - sin(a-b))/2
				addTrigTerm(r, true, plus, c);
				addTrigTerm(r, true, minus, neg(c));
			}
		}
	return r;
}

// sin(u)^n and cos(u)^n from ((e^(iu) -+ e^(-iu))/(2 or 2i))^n by the
// binomial theorem; the terms k and n-k give cos((n-2k)u) or sin((n-2k)u)
TrigSum trigPowerSum(bool isSin, const Expr& u, long n)
{
	TrigSum r;
	Rational binom(1), scale(1);
	for (long i = 0; i < n; i++)
		scale = scale / Rational(2);
	for (long k = 0; 2 * k <= n; k++) {
		long s = n - 2 * k;
		Rational c = binom * scale;
		if (isSin) {
			if (k % 2)
				c = -c;
			if ((n / 2) % 2) // (-1)^(n/2) for even n, (-1)^((n-1)/2) for odd n
				c = -c;
		}
		if (s > 0)
			c = c * Rational(2);
		Expr angle = mul(mkInt(s), u);
		if (isSin && n % 2)
			addTrigTerm(r, true, angle, mkNum(c));
		else
			addTrigTerm(r, false, angle, mkNum(c));
		binom = binom * Rational(n - k) / Rational(k + 1);
	}
	return r;
}

// sin(u)^n or cos(u)^n with 1 <= n <= 1000?
bool trigPower(const Expr& f, bool& isSin, Expr& u, long& n)
{
	Expr b = f;
	n = 1;
	if (f->kind == Kind::Pow) {
		b = f->args[0];
		if (!isInt(f->args[1], n) || n < 1 || n > 1000)
			return false;
	}
	if (!isCall(b, "sin") && !isCall(b, "cos"))
		return false;
	isSin = b->name == "sin";
	u = b->args[0];
	return true;
}

// How many sin/cos factors a term of e has at most (sin(x)^2*cos(x) -> 3)
long trigDegree(const Expr& e)
{
	switch (e->kind) {
	case Kind::Call:
		return isCall(e, "sin") || isCall(e, "cos") ? 1 : 0;
	case Kind::Pow: {
		long n;
		if (isInt(e->args[1], n) && n > 0)
			return n * std::min(trigDegree(e->args[0]), 1001L);
		return 0;
	}
	case Kind::Mul: {
		long d = 0;
		for (const Expr& f : e->args)
			d += std::min(trigDegree(f), 1001L);
		return d;
	}
	case Kind::Add: {
		long d = 0;
		for (const Expr& t : e->args)
			d = std::max(d, trigDegree(t));
		return d;
	}
	default:
		return 0;
	}
}

// one term of an expanded sum, written with its sin/cos factors combined
Expr reduceTerm(const Expr& t)
{
	Exprs factors = t->kind == Kind::Mul ? t->args : Exprs{t};
	Exprs rest;
	TrigSum acc{{TrigKey{false, mkInt(0)}, mkInt(1)}}; // cos(0) = 1
	for (const Expr& f : factors) {
		bool isSin;
		Expr u;
		long n;
		if (trigPower(f, isSin, u, n)) {
			acc = mulTrig(acc, trigPowerSum(isSin, u, n));
		} else {
			rest.push_back(f);
		}
	}
	Expr r = simpMul(rest);
	Exprs out;
	for (const auto& [k, c] : acc)
		out.push_back(mul(r, mul(c, k.isSin ? fsin(k.angle) : fcos(k.angle))));
	return simpAdd(out);
}

// sums of products of sin and cos written as sums of sin and cos; calls
// (and exponents) are left as they are
Expr linearize(const Expr& e)
{
	Expr r = e;
	switch (e->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(linearize(x));
		r = e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
		break;
	}
	case Kind::Pow:
		r = simpPow(linearize(e->args[0]), e->args[1]);
		break;
	default:
		return e;
	}
	if (trigDegree(r) < 2)
		return r;
	if (trigDegree(r) > 1000)
		throw Error("trigreduce: the powers of sin and cos are too large");
	Expr x = expand(r);
	Exprs terms = x->kind == Kind::Add ? x->args : Exprs{x};
	Exprs out;
	for (const Expr& t : terms)
		out.push_back(trigDegree(t) < 2 ? t : reduceTerm(t));
	return simpAdd(out);
}

// Is there a tan, or a sin or cos in a denominator (outside of calls)?
bool needsFraction(const Expr& e)
{
	long n;
	switch (e->kind) {
	case Kind::Call:
		return isCall(e, "tan");
	case Kind::Pow:
		if (isInt(e->args[1], n) && n < 0 && trigDegree(e->args[0]) > 0)
			return true;
		return needsFraction(e->args[0]);
	case Kind::Add:
	case Kind::Mul:
		for (const Expr& x : e->args)
			if (needsFraction(x))
				return true;
		return false;
	default:
		return false;
	}
}

// tan(u) -> sin(u)/cos(u) (outside of calls)
Expr tanToSinCos(const Expr& e)
{
	switch (e->kind) {
	case Kind::Call:
		if (isCall(e, "tan"))
			return mul(fsin(e->args[0]), simpPow(fcos(e->args[0]), mkInt(-1)));
		return e;
	case Kind::Pow:
		return simpPow(tanToSinCos(e->args[0]), e->args[1]);
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(tanToSinCos(x));
		return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	default:
		return e;
	}
}

// e = coef * sin(v) or coef * cos(v), with no other sin or cos in coef?
bool singleTrig(const Expr& e, bool& isSin, Expr& v, Expr& coef)
{
	Exprs factors = e->kind == Kind::Mul ? e->args : Exprs{e};
	Exprs rest;
	bool found = false;
	for (const Expr& f : factors) {
		if (!found && (isCall(f, "sin") || isCall(f, "cos"))) {
			found = true;
			isSin = f->name == "sin";
			v = f->args[0];
		} else if (trigDegree(f) > 0 || needsFraction(f)) {
			return false;
		} else {
			rest.push_back(f);
		}
	}
	coef = simpMul(rest);
	return found;
}

Expr dvNum(const Expr& e, const Expr& c) { return mul(e, simpPow(c, mkInt(-1))); }

// e = k*(1 + s*cos(w)) with s = +-1 and k a number?
bool onePlusCos(const Expr& e, int& s, Expr& w, Rational& k)
{
	if (e->kind != Kind::Add || e->args.size() != 2)
		return false;
	size_t ni = e->args[0]->kind == Kind::Num ? 0 : 1;
	if (e->args[ni]->kind != Kind::Num)
		return false;
	k = e->args[ni]->num;
	Expr t = e->args[1 - ni];
	Rational c(1);
	if (t->kind == Kind::Mul && t->args.size() == 2 && t->args[0]->kind == Kind::Num) {
		c = t->args[0]->num;
		t = t->args[1];
	}
	if (!isCall(t, "cos") || k.isZero() || c.abs().compare(k.abs()) != 0)
		return false;
	s = (c / k).sign();
	w = t->args[0];
	return true;
}

// p/q, with a*sin(v)/(b*cos(v)) written as (a/b)*tan(v), and the half-angle
// forms sin(2v)/(1+cos(2v)) = (1-cos(2v))/sin(2v) = tan(v)
Expr trigQuotient(const Expr& p, const Expr& q)
{
	bool ps, qs;
	Expr pv, qv, pc, qc;
	if (singleTrig(p, ps, pv, pc) && singleTrig(q, qs, qv, qc) && ps != qs && equal(pv, qv)) {
		Expr c = mul(pc, simpPow(qc, mkInt(-1)));
		Expr t = ftan(pv);
		return ps ? mul(c, t) : mul(c, simpPow(t, mkInt(-1)));
	}
	{
		int s1, s2;
		Expr w1, w2;
		Rational k1, k2;
		auto halfTan = [](const Expr& w) { return ftan(mul(mkNum(Rational(1) / Rational(2)), w)); };
		bool pOne = onePlusCos(p, s1, w1, k1), qOne = onePlusCos(q, s2, w2, k2);
		if (qOne && s2 > 0 && singleTrig(p, ps, pv, pc) && ps && equal(pv, w2))
			return mul(mul(pc, mkNum(Rational(1) / k2)), halfTan(w2)); // sin/(1+cos)
		if (pOne && s1 < 0 && singleTrig(q, qs, qv, qc) && qs && equal(qv, w1))
			return mul(mul(mkNum(k1), simpPow(qc, mkInt(-1))), halfTan(w1)); // (1-cos)/sin
	}
	// p/q with integer coefficients in q, without a common number, and a
	// positive constant term in q where it has one
	Exprs qt = q->kind == Kind::Add ? q->args : Exprs{q};
	Rational g(0);
	bool negConst = false;
	for (const Expr& t : qt) {
		if (t->kind == Kind::Num)
			negConst = t->num.sign() < 0;
		g = gcd(g, t->kind == Kind::Num ? t->num
		           : t->kind == Kind::Mul && t->args[0]->kind == Kind::Num ? t->args[0]->num
		                                                                  : Rational(1));
	}
	Expr c = mkNum(negConst ? -g : g);
	return mul(expand(dvNum(p, c)), simpPow(expand(dvNum(q, c)), mkInt(-1)));
}

Expr trigreduceTop(const Expr& e);

// the arguments of calls (and exponents) reduced
Expr reduceInside(const Expr& e)
{
	switch (e->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(reduceInside(x));
		return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(reduceInside(e->args[0]), trigreduceTop(e->args[1]));
	case Kind::Call: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(trigreduceTop(x));
		return isElementary(e->name) ? makeFunction(e->name, a) : mkCall(e->name, a);
	}
	default:
		return e;
	}
}

// tan as sin/cos, one fraction p/q, then p and q reduced on their own
Expr reduceFraction(const Expr& x)
{
	// Mostly the denominators are products of powers (from tan = sin/cos):
	// then the common denominator is their lcm, and no gcds are needed
	Expr y = expand(tanToSinCos(x));
	Exprs terms = y->kind == Kind::Add ? y->args : Exprs{y};
	std::vector<std::pair<Expr, long>> den; // base, largest negative power
	bool monomials = true;
	for (const Expr& t : terms) {
		Exprs fs = t->kind == Kind::Mul ? t->args : Exprs{t};
		for (const Expr& f : fs) {
			long k;
			if (f->kind != Kind::Pow || !isInt(f->args[1], k) || k >= 0)
				continue;
			const Expr& b = f->args[0];
			if (b->kind == Kind::Add) {
				monomials = false;
				break;
			}
			bool found = false;
			for (auto& [base, m] : den)
				if (equal(base, b)) {
					m = std::max(m, -k);
					found = true;
				}
			if (!found)
				den.emplace_back(b, -k);
		}
	}
	Expr p, q;
	if (monomials) {
		Exprs d;
		for (const auto& [b, k] : den)
			d.push_back(simpPow(b, mkInt(k)));
		q = simpMul(d);
		p = expand(mul(y, q));
		return trigQuotient(linearize(p), linearize(q));
	}
	std::tie(p, q) = cancelParts(y);
	p = linearize(p);
	q = linearize(q);
	if (q->kind != Kind::Num) {
		auto [p2, q2] = cancelParts(mul(p, simpPow(q, mkInt(-1))));
		p = p2;
		q = q2;
	}
	return trigQuotient(p, q);
}

long nodeCount(const Expr& e)
{
	long n = 1;
	for (const Expr& a : e->args)
		n += nodeCount(a);
	return n;
}

Expr trigreduceTop(const Expr& e)
{
	Expr x = reduceInside(e);
	if (!needsFraction(x))
		return linearize(x);
	Expr whole = reduceFraction(x);
	if (x->kind != Kind::Add)
		return whole;
	// a sum: term by term may be simpler (tan(x) + 1 stays as it is)
	Exprs terms;
	for (const Expr& t : x->args)
		terms.push_back(needsFraction(t) ? reduceFraction(t) : linearize(t));
	Expr byTerm = simpAdd(terms);
	return nodeCount(byTerm) <= nodeCount(whole) ? byTerm : whole;
}

} // namespace

Expr trigreduce(const Expr& e) { return trigreduceTop(e); }

} // namespace xyz
