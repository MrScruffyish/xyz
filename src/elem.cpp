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

Expr fsin(const Expr& u)
{
	Rational k;
	Expr rest;
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
	Expr rest;
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

} // namespace

bool isElementary(const std::string& n)
{
	return n == "sin" || n == "cos" || n == "tan" || n == "asin" || n == "acos" ||
	       n == "atan" || n == "exp" || n == "ln";
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
	throw Error("diff: cannot differentiate " + name + "()");
}

// ------------------------------------------------------------ trigexpand

namespace {

Expr trigexpandRec(const Expr& e);

// sin(n*u) and cos(n*u) from (cos(u) + im*sin(u))^n
std::pair<Expr, Expr> multipleAngle(long n, const Expr& u)
{
	Expr c = mkSym("~c"), s = mkSym("~s");
	Expr z = expand(simpPow(add(c, mul(mkIm(), s)), mkInt(n)));
	Expr re = realPart(z), im = imagPart(z);
	auto put = [&](const Expr& x) {
		// substitute ~c -> cos(u), ~s -> sin(u)
		struct R {
			const Expr &c, &s;
			Expr operator()(const Expr& x) const
			{
				if (x->kind == Kind::Sym)
					return x->name == "~c" ? c : x->name == "~s" ? s : x;
				if (x->args.empty())
					return x;
				auto m = std::make_shared<Node>(*x);
				for (Expr& a : m->args)
					a = (*this)(a);
				return simplify(m);
			}
		};
		Expr cu = fcos(u), su = fsin(u);
		return expand(R{cu, su}(x));
	};
	return {put(im), put(re)}; // sin, cos
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
		if (e->name == "tan") // tan = sin/cos
			return mul(expandSin(a[0]), simpPow(expandCos(a[0]), mkInt(-1)));
		return isElementary(e->name) ? makeFunction(e->name, a) : mkCall(e->name, a);
	}
	default:
		return e;
	}
}

} // namespace

Expr trigexpand(const Expr& e) { return trigexpandRec(e); }

} // namespace xyz
