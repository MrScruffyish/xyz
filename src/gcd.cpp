// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GCD of numbers and of multivariate polynomials, and cancel().
//
// Polynomials are expanded expressions with rational coefficients. The
// GCD is computed recursively: a polynomial in the main variable x has
// coefficients that are polynomials in the remaining variables.
//   gcd(a, b) = gcd(content(a), content(b)) * primitive PRS(pp(a), pp(b))
// where content is the GCD of the coefficients, pp the primitive part,
// and the primitive PRS is Euclid's algorithm with pseudo-remainders whose
// content is removed in every step.
#include "expr.hpp"

#include <set>

namespace xyz {

// ---------------------------------------------------------------- numbers

Rational gcd(const Rational& a, const Rational& b)
{
	BigInt n = BigInt::gcd(a.numerator(), b.numerator());
	BigInt d = (a.denominator() * b.denominator())
	               .divTrunc(BigInt::gcd(a.denominator(), b.denominator()));
	return Rational(n, d);
}

Rational lcm(const Rational& a, const Rational& b)
{
	if (a.isZero() || b.isZero())
		return Rational(0);
	BigInt n = (a.numerator() * b.numerator()).abs()
	               .divTrunc(BigInt::gcd(a.numerator(), b.numerator()));
	BigInt d = BigInt::gcd(a.denominator(), b.denominator());
	return Rational(n, d);
}

// ------------------------------------------------------------ polynomials

namespace {

void symbolsOf(const Expr& e, std::set<std::string>& out)
{
	if (e->kind == Kind::Sym)
		out.insert(e->name);
	for (const Expr& a : e->args)
		symbolsOf(a, out);
}

std::set<std::string> symbolsOf(const Expr& e)
{
	std::set<std::string> s;
	symbolsOf(e, s);
	return s;
}

void requirePolynomial(const Expr& e, const char* who)
{
	Exprs single{e};
	const Exprs& terms = e->kind == Kind::Add ? e->args : single;
	for (const Expr& t : terms) {
		Exprs one{t};
		const Exprs& fs = t->kind == Kind::Mul ? t->args : one;
		for (const Expr& f : fs) {
			long k;
			bool ok = f->kind == Kind::Num || f->kind == Kind::Sym ||
			          (f->kind == Kind::Pow && f->args[0]->kind == Kind::Sym &&
			           isInt(f->args[1], k) && k >= 0);
			if (!ok)
				throw Error(std::string(who) + ": not a polynomial: " + toString(e));
		}
	}
}

Expr powX(const std::string& x, long k) { return simpPow(mkSym(x), mkInt(k)); }

long degreeOf(const std::map<long, Expr>& c) { return c.empty() ? -1 : c.rbegin()->first; }

// The rational number c such that p/c has integer coefficients without a
// common factor and a positive leading (first printed) term.
Rational unitScale(const Expr& p)
{
	Exprs single{p};
	const Exprs& terms = p->kind == Kind::Add ? p->args : single;
	Rational g(0);
	for (const Expr& t : terms) {
		Rational c(1);
		if (t->kind == Kind::Num)
			c = t->num;
		else if (t->kind == Kind::Mul && t->args[0]->kind == Kind::Num)
			c = t->args[0]->num;
		g = gcd(g, c);
	}
	const Expr& lead = terms[0];
	bool negative = (lead->kind == Kind::Num && lead->num.sign() < 0) ||
	                (lead->kind == Kind::Mul && lead->args[0]->kind == Kind::Num &&
	                 lead->args[0]->num.sign() < 0);
	return negative ? -g : g;
}

Expr scale(const Expr& p, const Rational& c) { return expand(mul(mkNum(Rational(1) / c), p)); }

Expr normalized(const Expr& p)
{
	if (isZero(p))
		return p;
	return scale(p, unitScale(p));
}

// a / b for polynomials where b divides a exactly
Expr exactDiv(const Expr& a, const Expr& b)
{
	if (isZero(a))
		return a;
	if (b->kind == Kind::Num)
		return scale(a, b->num);
	std::string x = *symbolsOf(b).begin();
	auto bc = polyCoefficients(b, x);
	long db = degreeOf(bc);
	Expr lcb = bc.at(db);
	Expr q = mkInt(0), r = a;
	while (!isZero(r)) {
		auto rc = polyCoefficients(r, x);
		long dr = degreeOf(rc);
		if (dr < db)
			throw Error("internal error: polynomial division is not exact");
		Expr term = mul(exactDiv(rc.at(dr), lcb), powX(x, dr - db));
		q = add(q, term);
		r = expand(sub(r, expand(mul(term, b))));
	}
	return expand(q);
}

Expr pgcd(const Expr& a, const Expr& b);

// GCD of the coefficients of p as a polynomial in x
Expr content(const Expr& p, const std::string& x)
{
	Expr g = mkInt(0);
	for (const auto& [k, c] : polyCoefficients(p, x)) {
		g = pgcd(g, c);
		if (g->kind == Kind::Num)
			return mkInt(1); // a unit: nothing to remove
	}
	return g;
}

Expr primitivePart(const Expr& p, const std::string& x)
{
	if (isZero(p))
		return p;
	return exactDiv(p, content(p, x));
}

// pseudo-remainder of a by b in x (up to a factor without x)
Expr prem(const Expr& a, const Expr& b, const std::string& x)
{
	auto bc = polyCoefficients(b, x);
	long db = degreeOf(bc);
	Expr lcb = bc.at(db);
	Expr r = a;
	for (;;) {
		auto rc = polyCoefficients(r, x);
		long dr = degreeOf(rc);
		if (dr < db)
			return r;
		Expr lead = mul(rc.at(dr), powX(x, dr - db));
		r = expand(sub(mul(lcb, r), mul(lead, b)));
	}
}

Expr pgcd(const Expr& a, const Expr& b)
{
	if (isZero(a))
		return normalized(b);
	if (isZero(b))
		return normalized(a);
	std::set<std::string> vars = symbolsOf(a);
	for (const std::string& v : symbolsOf(b))
		vars.insert(v);
	if (vars.empty())
		return mkInt(1); // nonzero constants: a unit
	const std::string& x = *vars.begin();
	bool inA = containsSym(a, x), inB = containsSym(b, x);
	if (!inA)
		return pgcd(a, content(b, x));
	if (!inB)
		return pgcd(content(a, x), b);
	Expr ca = content(a, x), cb = content(b, x);
	Expr c = pgcd(ca, cb);
	Expr p = exactDiv(a, ca), q = exactDiv(b, cb);
	if (polyDegree(p, x) < polyDegree(q, x))
		std::swap(p, q);
	while (!isZero(q)) {
		// constants are units over the rationals: scaling every remainder
		// to small integer coefficients keeps the numbers from growing
		Expr r = normalized(prem(p, q, x));
		p = q;
		q = primitivePart(r, x);
	}
	return normalized(expand(mul(c, primitivePart(p, x))));
}

// --------------------------------------------------------------- fractions

struct Frac {
	Expr n, d; // expanded polynomials, d != 0
};

Frac reduce(const Frac& f)
{
	if (isZero(f.n))
		return {f.n, mkInt(1)};
	Expr g = pgcd(f.n, f.d);
	Expr n = exactDiv(f.n, g), d = exactDiv(f.d, g);
	Rational s = unitScale(d); // denominator: integer, positive leading term
	return {scale(n, s), scale(d, s)};
}

// Non-polynomial parts are replaced by fresh symbols (~1, ~2, ...).
struct Atoms {
	std::vector<std::pair<Expr, std::string>> list;

	Expr symbolFor(const Expr& e)
	{
		for (const auto& [a, name] : list)
			if (equal(a, e))
				return mkSym(name);
		// '~' sorts after all names, so atoms come last in sums
		list.emplace_back(e, "~" + std::to_string(list.size() + 1));
		return mkSym(list.back().second);
	}

	Expr put(const Expr& e)
	{
		long k;
		switch (e->kind) {
		case Kind::Num:
			return e;
		case Kind::Sym:
			// im is kept apart: the polynomial algorithms do not know im^2 = -1
			return isImUnit(e) ? symbolFor(e) : e;
		case Kind::Add:
		case Kind::Mul: {
			Exprs a;
			for (const Expr& x : e->args)
				a.push_back(put(x));
			return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
		}
		case Kind::Pow:
			if (isInt(e->args[1], k))
				return simpPow(put(e->args[0]), e->args[1]);
			return symbolFor(e);
		default:
			return symbolFor(e);
		}
	}

	Expr back(const Expr& e) const
	{
		if (e->kind == Kind::Sym) {
			for (const auto& [a, name] : list)
				if (name == e->name)
					return a;
			return e;
		}
		if (e->args.empty())
			return e;
		auto n = std::make_shared<Node>(*e);
		for (Expr& a : n->args)
			a = back(a);
		return simplify(n);
	}
};

Frac normal(const Expr& e)
{
	switch (e->kind) {
	case Kind::Num:
	case Kind::Sym:
		return {e, mkInt(1)};
	case Kind::Add: {
		Frac acc{mkInt(0), mkInt(1)};
		for (const Expr& t : e->args) {
			Frac f = normal(t);
			acc = reduce({expand(add(mul(acc.n, f.d), mul(f.n, acc.d))),
			              expand(mul(acc.d, f.d))});
		}
		return acc;
	}
	case Kind::Mul: {
		Frac acc{mkInt(1), mkInt(1)};
		for (const Expr& t : e->args) {
			Frac f = normal(t);
			acc = reduce({expand(mul(acc.n, f.n)), expand(mul(acc.d, f.d))});
		}
		return acc;
	}
	case Kind::Pow: {
		Frac b = normal(e->args[0]);
		long k = 0;
		isInt(e->args[1], k); // only integer powers are left (see Atoms)
		if (k >= 0)
			return {expand(simpPow(b.n, mkInt(k))), expand(simpPow(b.d, mkInt(k)))};
		if (isZero(b.n))
			throw Error("division by zero");
		return reduce({expand(simpPow(b.d, mkInt(-k))), expand(simpPow(b.n, mkInt(-k)))});
	}
	default:
		throw Error("cancel: unsupported expression " + toString(e));
	}
}

} // namespace

// only im is replaced by a symbol: roots and the like are not polynomials
void onlyIm(const Expr& e, const char* who)
{
	Atoms atoms;
	atoms.put(e);
	for (const auto& atom : atoms.list)
		if (!isImUnit(atom.first))
			throw Error(std::string(who) + ": not a polynomial: " + toString(e));
}

Expr polyGcd(const Expr& a, const Expr& b)
{
	Atoms atoms;
	Expr x = expand(atoms.put(a)), y = expand(atoms.put(b));
	onlyIm(a, "gcd");
	onlyIm(b, "gcd");
	requirePolynomial(x, "gcd");
	requirePolynomial(y, "gcd");
	return expand(atoms.back(pgcd(x, y)));
}

Expr polyLcm(const Expr& a, const Expr& b)
{
	Atoms atoms;
	Expr x = expand(atoms.put(a)), y = expand(atoms.put(b));
	onlyIm(a, "lcm");
	onlyIm(b, "lcm");
	requirePolynomial(x, "lcm");
	requirePolynomial(y, "lcm");
	if (isZero(x) || isZero(y))
		return mkInt(0);
	return expand(atoms.back(normalized(exactDiv(expand(mul(x, y)), pgcd(x, y)))));
}

Expr cancel(const Expr& e)
{
	if (e->kind == Kind::Dec)
		throw Error("a num() result cannot be used in further calculations");
	Atoms atoms;
	Frac f = reduce(normal(atoms.put(e)));
	Expr r = f.d->kind == Kind::Num ? scale(f.n, f.d->num)
	                                 : mul(f.n, simpPow(f.d, mkInt(-1)));
	return atoms.back(r);
}

bool isIdenticallyZero(const Expr& e)
{
	if (isZero(expand(e)))
		return true;
	if (!hasVariableDenominator(e))
		return false; // the expanded form of a polynomial is unique
	Atoms atoms;
	Expr n = normal(atoms.put(e)).n;
	// the atoms may still cancel once they are put back (im^2 = -1)
	return isZero(n) || isZero(expand(atoms.back(n)));
}

} // namespace xyz
