// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// taylor() and limit().
//
// limit(f, x = a): substitution where f is defined; otherwise the limits
// of the parts are combined, with L'Hopital's rule for 0/0 and inf/inf,
// 0*inf written as a quotient, b^e as exp(e*ln(b)), and the limits of the
// functions at the ends of their domains (ln(0+) = -inf, atan(inf) = pi/2).
// For x -> inf, algebraic expressions (powers and roots of polynomials) go
// by their order of growth and leading coefficient, and inf - inf with a
// square root by A - B = (A^2 - B^2)/(A + B). x -> -inf is x = -y with
// y -> inf. The sign of an infinite limit is found numerically, close to
// the point. It is not a complete method: it gives an error when it cannot
// decide.
#include "expr.hpp"

#include <functional>

namespace xyz {

namespace {

Expr infSym() { return mkSym("inf"); }
Expr neg(const Expr& e) { return mul(mkInt(-1), e); }
Expr dv(const Expr& a, const Expr& b) { return mul(a, simpPow(b, mkInt(-1))); }

// +1 for inf, -1 for -inf, 0 otherwise
int infSign(const Expr& e)
{
	if (e->kind == Kind::Sym && e->name == "inf")
		return 1;
	if (e->kind == Kind::Mul && e->args.size() == 2 && e->args[0]->kind == Kind::Num &&
	    e->args[1]->kind == Kind::Sym && e->args[1]->name == "inf")
		return e->args[0]->num.sign();
	return 0;
}

Expr signedInf(int s) { return s > 0 ? infSym() : neg(infSym()); }

bool hasInf(const Expr& e) { return containsSym(e, "inf"); }

struct NoLimit : Error {
	using Error::Error;
};

// (b^r)^n -> b^(r*n) for integer n
Expr flatPow(const Expr& n)
{
	switch (n->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& c : n->args)
			a.push_back(flatPow(c));
		return n->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow: {
		Expr b = flatPow(n->args[0]);
		long m;
		if (b->kind == Kind::Pow && isInt(n->args[1], m))
			return simpPow(b->args[0], mul(b->args[1], n->args[1]));
		return simpPow(b, n->args[1]);
	}
	default:
		return n;
	}
}

struct Limit {
	std::string x;
	Expr a;  // a finite point, or inf
	int dir; // +1 from the right, -1 from the left, 0 both
	bool atInf() const { return infSign(a) != 0; }

	// the sign of f close to a, on the side s (+1 or -1); 0 if unknown
	int signNear(const Expr& f, int s)
	{
		for (int k : {12, 24}) {
			Expr eps = simpPow(mkInt(10), mkInt(-k));
			Expr pt = atInf() ? simpPow(mkInt(10), mkInt(k)) : add(a, mul(mkInt(s), eps));
			try {
				Expr v = numeric(substitute(f, x, pt), 30);
				if (v->kind != Kind::Dec)
					return v->kind == Kind::Num ? v->num.sign() : 0;
				const std::string& t = v->name;
				if (t.find(imagName()) != std::string::npos)
					return 0;
				if (t.find_first_not_of("-0.") == std::string::npos)
					continue; // looks like 0: closer
				return t[0] == '-' ? -1 : 1;
			} catch (const Error&) {
			}
		}
		return 0;
	}

	// +-inf for f -> +-inf near a (a pole); the sides must agree
	Expr pole(const Expr& f)
	{
		if (dir != 0 || atInf()) {
			int s = signNear(f, dir);
			if (s == 0)
				throw NoLimit("limit: cannot decide the sign of " + toString(f));
			return signedInf(s);
		}
		int l = signNear(f, -1), r = signNear(f, 1);
		if (l == 0 || r == 0)
			throw NoLimit("limit: cannot decide the sign of " + toString(f));
		if (l != r)
			throw NoLimit("limit: does not exist (-inf on one side, inf on the other); "
			              "try limit(f, x = a, left) or right");
		return signedInf(l);
	}

	Expr lim(const Expr& f, int depth)
	{
		if (depth > 30)
			throw NoLimit("limit: cannot find the limit (too deep)");
		if (!containsSym(f, x))
			return f;
		if (f->kind == Kind::Sym)
			return a;
		if (atInf()) {
			Rational ord;
			Expr lc;
			if (growth(f, ord, lc)) {
				if (ord.sign() < 0)
					return mkInt(0);
				if (ord.isZero())
					return lc;
				if (lc->kind == Kind::Num)
					return signedInf(lc->num.sign());
				return pole(f);
			}
		} else {
			try { // where f is defined: its value
				Expr v = substitute(f, x, a);
				if (!hasInf(v))
					return v;
			} catch (const Error&) {
			}
		}
		Expr g = f;
		if (!atInf() && isRationalFn(g) && hasVariableDenominator(g)) {
			g = cancel(g);
			try {
				Expr v = substitute(g, x, a);
				if (!hasInf(v))
					return v;
			} catch (const Error&) {
			}
		}
		switch (g->kind) {
		case Kind::Add:
			return limSum(g, depth);
		case Kind::Mul:
			return limProduct(g, depth);
		case Kind::Pow:
			return limPow(g, depth);
		case Kind::Call:
			return limCall(g, depth);
		default:
			throw NoLimit("limit: cannot find the limit of " + toString(f));
		}
	}

	// f ~ lc * x^ord for x -> inf (f algebraic: sums, products and
	// numeric powers of x and constants); false when unknown or cancelling
	bool growth(const Expr& f, Rational& ord, Expr& lc)
	{
		switch (f->kind) {
		case Kind::Num:
			ord = Rational(0);
			lc = f;
			return !isZero(f);
		case Kind::Sym:
			ord = Rational(f->name == x ? 1 : 0);
			lc = f->name == x ? mkInt(1) : f;
			return true;
		case Kind::Mul: {
			ord = Rational(0);
			lc = mkInt(1);
			for (const Expr& c : f->args) {
				Rational o;
				Expr l;
				if (!growth(c, o, l))
					return false;
				ord = ord + o;
				lc = mul(lc, l);
			}
			return true;
		}
		case Kind::Pow: {
			if (f->args[1]->kind != Kind::Num || !containsSym(f->args[0], x)) {
				if (containsSym(f, x))
					return false;
				ord = Rational(0);
				lc = f;
				return true;
			}
			Rational o;
			Expr l;
			if (!growth(f->args[0], o, l))
				return false;
			const Rational& r = f->args[1]->num;
			if (!r.isInt() && !(l->kind == Kind::Num && l->num.sign() > 0))
				return false;
			ord = o * r;
			lc = simpPow(l, f->args[1]);
			return true;
		}
		case Kind::Add: {
			bool first = true;
			Exprs top;
			for (const Expr& t : f->args) {
				Rational o;
				Expr l;
				if (!growth(t, o, l))
					return false;
				if (first || o.compare(ord) > 0) {
					ord = o;
					top = {l};
					first = false;
				} else if (o.compare(ord) == 0) {
					top.push_back(l);
				}
			}
			lc = simpAdd(top);
			return !isIdenticallyZero(lc); // cancelling: unknown
		}
		default:
			if (containsSym(f, x))
				return false;
			ord = Rational(0);
			lc = f;
			return true;
		}
	}

	bool hasFunctionOfX(const Expr& e)
	{
		if (e->kind == Kind::Call && containsSym(e, x))
			return true;
		for (const Expr& c : e->args)
			if (hasFunctionOfX(c))
				return true;
		return false;
	}

	// exp(...x...) or c^(...x...)
	bool hasExpOfX(const Expr& e)
	{
		if ((e->kind == Kind::Call && e->name == "exp" && containsSym(e, x)) ||
		    (e->kind == Kind::Pow && containsSym(e->args[1], x)))
			return true;
		for (const Expr& c : e->args)
			if (hasExpOfX(c))
				return true;
		return false;
	}

	bool isRationalFn(const Expr& f)
	{
		long k;
		switch (f->kind) {
		case Kind::Num:
		case Kind::Sym:
			return true;
		case Kind::Add:
		case Kind::Mul:
			for (const Expr& c : f->args)
				if (!isRationalFn(c))
					return false;
			return true;
		case Kind::Pow:
			return !containsSym(f, x) || (isInt(f->args[1], k) && isRationalFn(f->args[0]));
		default:
			return !containsSym(f, x);
		}
	}

	Expr limSum(const Expr& f, int depth)
	{
		Exprs finite;
		int infs = 0;
		for (const Expr& t : f->args) {
			Expr l;
			try {
				l = lim(t, depth + 1);
			} catch (const NoLimit&) {
				return quotient(f, depth); // maybe the terms cancel
			}
			int s = infSign(l);
			if (s == 0) {
				finite.push_back(l);
			} else if (infs == 0 || infs == s) {
				infs = s;
			} else { // inf - inf
				return infMinusInf(f, depth);
			}
		}
		return infs ? signedInf(infs) : simpAdd(finite);
	}

	// A - B = (A^2 - B^2)/(A + B) for a square root A; else one fraction
	Expr infMinusInf(const Expr& f, int depth)
	{
		for (const Expr& t : f->args) {
			bool root = false;
			for (const Expr& c : t->kind == Kind::Mul ? t->args : Exprs{t})
				root = root || (c->kind == Kind::Pow && c->args[1]->kind == Kind::Num &&
				                c->args[1]->num.denominator().toString() == "2");
			if (!root)
				continue;
			Expr A = t, B = sub(A, f); // f = A - B
			Expr top = expand(flatPow(sub(simpPow(A, mkInt(2)), simpPow(B, mkInt(2)))));
			return lim(dv(top, add(A, B)), depth + 1);
		}
		return quotient(f, depth);
	}

	// f = p/q, from the factors with negative powers
	void split(const Expr& f, Expr& p, Expr& q)
	{
		Exprs ps, qs;
		for (const Expr& c : f->kind == Kind::Mul ? f->args : Exprs{f}) {
			if (c->kind == Kind::Pow && c->args[1]->kind == Kind::Num && c->args[1]->num.sign() < 0)
				qs.push_back(simpPow(c->args[0], mkNum(-c->args[1]->num)));
			else
				ps.push_back(c);
		}
		p = simpMul(ps);
		q = simpMul(qs);
	}

	// p/q with L'Hopital's rule for 0/0 and inf/inf
	Expr quotient(const Expr& f, int depth)
	{
		Expr p, q;
		split(f, p, q);
		if (isNumber(q, 1)) { // a sum without a denominator: one fraction
			auto [n, d] = cancelParts(f);
			p = n;
			q = d;
		}
		if (!containsSym(q, x))
			return dv(lim(p, depth + 1), q);
		Expr lp = lim(p, depth + 1), lq = lim(q, depth + 1);
		int sp = infSign(lp), sq = infSign(lq);
		if (sq == 0 && !isZero(lq)) { // finite, nonzero denominator
			if (sp)
				return pole(f);
			return dv(lp, lq);
		}
		if (sq != 0 && sp == 0)
			return mkInt(0); // finite/inf
		if (isZero(lq) && !isZero(lp) && sp == 0)
			return pole(f); // c/0
		// 0/0 or inf/inf (or inf/0): L'Hopital
		Expr dp = diff(p, x), dq = diff(q, x);
		if (isZero(dq))
			throw NoLimit("limit: cannot find the limit of " + toString(f));
		Expr r = dv(dp, dq);
		if (isRationalFn(r) && hasVariableDenominator(r))
			r = cancel(r);
		return lim(r, depth + 1);
	}

	Expr limProduct(const Expr& f, int depth)
	{
		Expr p, q;
		split(f, p, q);
		if (!isNumber(q, 1))
			return quotient(f, depth);
		Exprs finite;
		int infs = 1;
		bool anyInf = false, anyZero = false;
		for (const Expr& c : f->args) {
			Expr l = lim(c, depth + 1);
			int s = infSign(l);
			if (s) {
				anyInf = true;
				infs *= s;
			} else {
				if (isZero(l))
					anyZero = true;
				finite.push_back(l);
			}
		}
		if (!anyInf)
			return simpMul(finite);
		if (anyZero) { // 0*inf: h/(1/g) (ln(x)/(1/x)), else g/(1/h)
			Exprs zero, big;
			for (const Expr& c : f->args)
				(infSign(lim(c, depth + 1)) ? big : zero).push_back(c);
			// ln-like parts go on top, exp-like parts below:
			// x*ln(x) = ln(x)/(1/x), x^2*exp(-x) = x^2/exp(x)
			Expr g = simpMul(zero), h = simpMul(big);
			bool gTop = (hasFunctionOfX(g) && !hasExpOfX(g)) || hasExpOfX(h) ||
			            (!hasFunctionOfX(h) && !hasExpOfX(g));
			try {
				return gTop ? quotientOf(g, simpPow(h, mkInt(-1)), depth)
				            : quotientOf(h, simpPow(g, mkInt(-1)), depth);
			} catch (const NoLimit&) {
				return gTop ? quotientOf(h, simpPow(g, mkInt(-1)), depth)
				            : quotientOf(g, simpPow(h, mkInt(-1)), depth);
			}
		}
		Expr c = simpMul(finite);
		if (c->kind == Kind::Num)
			return signedInf(infs * c->num.sign());
		return pole(f);
	}

	// lim n/d by L'Hopital where n -> 0 and d -> 0 (or both inf)
	Expr quotientOf(const Expr& n, const Expr& d, int depth)
	{
		Expr dn = diff(n, x), dd = diff(d, x);
		if (isZero(dd))
			throw NoLimit("limit: cannot find the limit");
		Expr r = dv(dn, dd);
		if (isRationalFn(r) && hasVariableDenominator(r))
			r = cancel(r);
		return lim(r, depth + 1);
	}

	Expr limPow(const Expr& f, int depth)
	{
		const Expr& b = f->args[0];
		const Expr& e = f->args[1];
		if (containsSym(e, x)) { // b^e = exp(e*ln(b))
			Expr l = lim(mul(e, makeFunction("ln", {b})), depth + 1);
			return expOf(l);
		}
		if (e->kind == Kind::Num && e->num.sign() < 0)
			return quotient(f, depth);
		Expr lb = lim(b, depth + 1);
		int s = infSign(lb);
		if (s) {
			if (e->kind != Kind::Num)
				throw NoLimit("limit: cannot find the limit of " + toString(f));
			if (s < 0) {
				long k;
				if (isInt(e, k))
					return signedInf(k % 2 ? -1 : 1);
				throw NoLimit("limit: (-inf)^" + toString(e) + " is not real");
			}
			return infSym();
		}
		return simpPow(lb, e);
	}

	Expr expOf(const Expr& l)
	{
		int s = infSign(l);
		if (s > 0)
			return infSym();
		if (s < 0)
			return mkInt(0);
		return makeFunction("exp", {l});
	}

	Expr limCall(const Expr& f, int depth)
	{
		const std::string& n = f->name;
		if (f->args.size() != 1 || !isElementary(n))
			throw NoLimit("limit: cannot find the limit of " + toString(f));
		Expr u = f->args[0];
		Expr l = lim(u, depth + 1);
		int s = infSign(l);
		if (n == "exp")
			return expOf(l);
		if (s == 0) {
			if (n == "ln" && isZero(l)) {
				// ln(u) with u -> 0: -inf when u > 0 near a
				return neg(infSym());
			}
			try {
				return makeFunction(n, {l});
			} catch (const Error&) {
				// tan(pi/2), atanh(1): a pole of the function
				return pole(f);
			}
		}
		if (n == "ln" && s < 0)
			throw NoLimit("limit: ln(-inf) is not real");
		if (n == "ln" || n == "sinh" || n == "asinh")
			return signedInf(s);
		if (n == "cosh" || n == "acosh")
			return infSym();
		if (n == "atan")
			return mul(mkNum(Rational(s) / Rational(2)), mkSym("pi"));
		if (n == "tanh")
			return mkInt(s);
		throw NoLimit("limit: " + n + "(" + (s > 0 ? "inf" : "-inf") + ") does not exist");
	}
};

} // namespace

Expr limit(const Expr& f, const std::string& x, const Expr& a, int dir)
{
	int s = infSign(a);
	if (s < 0) { // x = -y, y -> inf
		std::string y = "~y";
		Limit L{y, infSym(), 0};
		return L.lim(substitute(f, x, neg(mkSym(y))), 0);
	}
	if (s > 0) {
		Limit L{x, infSym(), 0};
		return L.lim(f, 0);
	}
	if (hasInf(a))
		throw Error("limit: the point must be a number, an expression, inf or -inf");
	Limit L{x, a, dir};
	return L.lim(f, 0);
}

Expr taylor(const Expr& f, const std::string& x, const Expr& a, long n)
{
	Exprs terms;
	Expr d = f;
	Rational fact(1);
	Expr h = sub(mkSym(x), a);
	for (long k = 0; k <= n; k++) {
		if (k > 0) {
			d = diff(d, x);
			fact = fact * Rational(k);
		}
		Expr v;
		try {
			v = substitute(d, x, a);
		} catch (const Error&) {
			v = limit(d, x, a, 0);
		}
		if (hasInf(v))
			throw Error("taylor: " + toString(f) + " has no Taylor series at " + x + " = " +
			            toString(a));
		terms.push_back(mul(mul(mkNum(Rational(1) / fact), v), simpPow(h, mkInt(k))));
	}
	return simpAdd(terms);
}

} // namespace xyz
