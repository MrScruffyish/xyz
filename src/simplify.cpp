// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Automatic simplification, expansion, differentiation and simple
// polynomial operations.
//
// Canonical form produced by simpAdd/simpMul/simpPow:
//  - sums are flat, like terms are combined (2*x+3*x -> 5*x), the numeric
//    constant comes last, other terms are ordered by total degree (highest
//    first) and then lexicographically (x before y, higher powers first).
//  - products are flat, the numeric coefficient comes first, equal bases
//    are merged by adding exponents (x*x -> x^2), factors ordered by base.
//    A number times a single sum is distributed: 2*(x+1) -> 2*x+2.
//  - powers: numbers with integer exponents are evaluated exactly,
//    (a^k)^n -> a^(k*n) for integers k and n, and (a*b)^n -> a^n*b^n
//    for integer n. Roots are not combined: (x^(1/2))^2 stays.
//  - products of sums and powers of sums are NOT multiplied out; that is
//    what expand() is for.
#include "expr.hpp"

#include <algorithm>
#include <map>

namespace xyz {

namespace {

// Set while safeSimplify() runs: calls are allowed as opaque atoms and
// nothing that removes a division is done.
thread_local bool g_safe = false;

struct SafeMode {
	bool old;
	SafeMode() : old(g_safe) { g_safe = true; }
	~SafeMode() { g_safe = old; }
};

void checkComputable(const Expr& e)
{
	if (e->kind == Kind::Dec)
		throw Error("a num() result cannot be used in further calculations");
	if (!g_safe && (e->kind == Kind::Call || e->kind == Kind::Arg))
		throw std::logic_error("unevaluated call reached the simplifier");
}

// Does e divide by something that could be zero?
bool hasDenominator(const Expr& e)
{
	if (e->kind == Kind::Pow && e->args[0]->kind != Kind::Num &&
	    e->args[1]->kind == Kind::Num && e->args[1]->num.sign() < 0)
		return true;
	for (const Expr& a : e->args)
		if (hasDenominator(a))
			return true;
	return false;
}

// Base and exponent of a factor (exponent nullptr means 1). Only integer
// exponents (or any exponent of a positive number) are split off, so
// x^(1/2) is a base of its own: x^(1/2)*x^(1/2) -> (x^(1/2))^2, not x.
std::pair<Expr, Expr> baseExp(const Expr& f)
{
	if (f->kind == Kind::Pow) {
		const Expr& b = f->args[0];
		const Expr& e = f->args[1];
		bool intExp = e->kind == Kind::Num && e->num.isInt();
		bool posBase = b->kind == Kind::Num && b->num.sign() > 0;
		if (intExp || posBase)
			return {b, e};
	}
	return {f, nullptr};
}

// the factors of a term without its numeric coefficient
Exprs termFactors(const Expr& t)
{
	if (t->kind == Kind::Mul) {
		size_t s = t->args[0]->kind == Kind::Num ? 1 : 0;
		return Exprs(t->args.begin() + static_cast<long>(s), t->args.end());
	}
	if (t->kind == Kind::Num)
		return {};
	return {t};
}

Rational termDegree(const Exprs& factors)
{
	Rational d;
	for (const Expr& f : factors) {
		auto [b, e] = baseExp(f);
		if (!e)
			d = d + Rational(1);
		else if (e->kind == Kind::Num)
			d = d + e->num;
	}
	return d;
}

// order of terms in a sum: true if a comes before b
bool termBefore(const Expr& a, const Expr& b)
{
	Exprs fa = termFactors(a), fb = termFactors(b);
	if (int c = termDegree(fa).compare(termDegree(fb)))
		return c > 0;
	Expr one = mkInt(1);
	for (size_t i = 0; i < fa.size() && i < fb.size(); i++) {
		auto [ba, ea] = baseExp(fa[i]);
		auto [bb, eb] = baseExp(fb[i]);
		if (int c = compare(ba, bb))
			return c < 0;
		if (int c = compare(ea ? ea : one, eb ? eb : one))
			return c > 0;
	}
	if (fa.size() != fb.size())
		return fa.size() > fb.size();
	return compare(a, b) < 0;
}

bool factorBefore(const Expr& a, const Expr& b)
{
	if (int c = compare(baseExp(a).first, baseExp(b).first))
		return c < 0;
	return compare(a, b) < 0;
}

struct ExprLess {
	bool operator()(const Expr& a, const Expr& b) const { return compare(a, b) < 0; }
};

Exprs flatten(const Exprs& in, Kind k)
{
	Exprs out;
	for (const Expr& e : in) {
		checkComputable(e);
		if (e->kind == k)
			out.insert(out.end(), e->args.begin(), e->args.end());
		else
			out.push_back(e);
	}
	return out;
}

Expr withCoeff(const Rational& c, const Expr& rest)
{
	Exprs f{mkNum(c)};
	if (rest->kind == Kind::Mul)
		f.insert(f.end(), rest->args.begin(), rest->args.end());
	else
		f.push_back(rest);
	return mkNode(Kind::Mul, std::move(f));
}

} // namespace

Expr add(const Expr& a, const Expr& b) { return simpAdd({a, b}); }
Expr mul(const Expr& a, const Expr& b) { return simpMul({a, b}); }
Expr sub(const Expr& a, const Expr& b) { return add(a, mul(mkInt(-1), b)); }

// ------------------------------------------------------------------ sums

Expr simpAdd(const Exprs& in)
{
	Rational konst;
	// rest -> summed coefficient; ordered so equal rests merge
	std::map<Expr, Rational, ExprLess> terms;
	for (const Expr& t : flatten(in, Kind::Add)) {
		if (t->kind == Kind::Num) {
			konst = konst + t->num;
			continue;
		}
		Rational c(1);
		Expr rest = t;
		if (t->kind == Kind::Mul && t->args[0]->kind == Kind::Num) {
			c = t->args[0]->num;
			rest = t->args.size() == 2
			           ? t->args[1]
			           : mkNode(Kind::Mul, Exprs(t->args.begin() + 1, t->args.end()));
		}
		auto it = terms.find(rest);
		if (it == terms.end())
			terms.emplace(rest, c);
		else
			it->second = it->second + c;
	}
	Exprs out;
	for (auto& [rest, c] : terms) {
		if (c.isZero()) {
			if (g_safe && hasDenominator(rest)) // 1/x-1/x stays 0/x
				out.push_back(withCoeff(c, rest));
			continue;
		}
		out.push_back(c.isOne() ? rest : withCoeff(c, rest));
	}
	std::sort(out.begin(), out.end(), termBefore);
	if (!konst.isZero())
		out.push_back(mkNum(konst));
	if (out.empty())
		return mkInt(0);
	if (out.size() == 1)
		return out[0];
	return mkNode(Kind::Add, std::move(out));
}

// -------------------------------------------------------------- products

Expr simpMul(const Exprs& in)
{
	Rational coeff(1);
	std::map<Expr, Exprs, ExprLess> bases; // base -> exponents
	for (const Expr& f : flatten(in, Kind::Mul)) {
		if (f->kind == Kind::Num) {
			coeff = coeff * f->num;
			continue;
		}
		auto [b, e] = baseExp(f);
		bases[b].push_back(e ? e : mkInt(1));
	}
	if (coeff.isZero() && !(g_safe && hasDenominator(mkNode(Kind::Mul, in))))
		return mkInt(0);
	Exprs out;
	bool again = false;
	auto emit = [&](const Expr& b, const Exprs& exps) {
		if (exps.empty())
			return;
		Expr e = exps.size() == 1 ? exps[0] : simpAdd(exps);
		Expr p = simpPow(b, e);
		if (p->kind == Kind::Num)
			coeff = coeff * p->num;
		else {
			again = again || p->kind == Kind::Mul;
			out.push_back(p);
		}
	};
	for (auto& [b, exps] : bases) {
		if (!g_safe || b->kind == Kind::Num) {
			emit(b, exps);
			continue;
		}
		// safe: x^2*x^-1 stays x^2/x (cancelling would change the domain)
		Exprs pos, neg;
		for (const Expr& e : exps)
			(e->kind == Kind::Num && e->num.sign() < 0 ? neg : pos).push_back(e);
		emit(b, pos);
		emit(b, neg);
	}
	if (again) { // a power of a product produced new factors: merge again
		out.push_back(mkNum(coeff));
		return simpMul(out);
	}
	if (coeff.isZero() && !(g_safe && !out.empty() && hasDenominator(mkNode(Kind::Mul, out))))
		return mkInt(0);
	if (out.empty())
		return mkNum(coeff);
	if (out.size() == 1 && coeff.isOne())
		return out[0];
	if (out.size() == 1 && out[0]->kind == Kind::Add) {
		Exprs terms;
		Expr c = mkNum(coeff);
		for (const Expr& t : out[0]->args)
			terms.push_back(mul(c, t));
		return simpAdd(terms);
	}
	std::sort(out.begin(), out.end(), factorBefore);
	if (!coeff.isOne())
		out.insert(out.begin(), mkNum(coeff));
	return mkNode(Kind::Mul, std::move(out));
}

// ---------------------------------------------------------------- powers

Expr simpPow(const Expr& b, const Expr& e)
{
	checkComputable(b);
	checkComputable(e);
	if (e->kind == Kind::Num) {
		long k = 0;
		bool eint = e->num.isInt();
		bool fits = e->num.toLong(k);
		if (b->kind == Kind::Num) {
			if (b->num.isZero()) {
				if (e->num.isZero())
					return mkInt(1); // 0^0 = 1, as in bc
				if (e->num.sign() < 0)
					throw Error("division by zero");
				return mkInt(0);
			}
			if (b->num.isOne())
				return mkInt(1);
			if (eint) {
				if (!fits)
					throw Error("exponent too large");
				return mkNum(b->num.pow(k));
			}
			return mkPow(b, e);
		}
		if (e->num.isZero() && !(g_safe && hasDenominator(b)))
			return mkInt(1);
		if (e->num.isOne())
			return b;
		// x^(-1/2) -> (x^(1/2))^-1, so that it cancels against x^(1/2)
		if (!eint && e->num.sign() < 0)
			return mkPow(simpPow(b, mkNum(-e->num)), mkInt(-1));
		if (eint) {
			// (a^k)^n -> a^(k*n) only for integer k: (x^(1/2))^2 is
			// not simplified to x (roots are decided later)
			if (b->kind == Kind::Pow && b->args[1]->kind == Kind::Num &&
			    b->args[1]->num.isInt())
				return simpPow(b->args[0], mul(b->args[1], e));
			if (b->kind == Kind::Mul) {
				Exprs f;
				for (const Expr& x : b->args)
					f.push_back(simpPow(x, e));
				return simpMul(f);
			}
		}
		return mkPow(b, e);
	}
	if (b->kind == Kind::Num && b->num.isOne())
		return mkInt(1);
	return mkPow(b, e);
}

Expr safeSimplify(const Expr& n)
{
	SafeMode guard;
	switch (n->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : n->args)
			a.push_back(safeSimplify(x));
		return n->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(safeSimplify(n->args[0]), safeSimplify(n->args[1]));
	case Kind::Call:
	case Kind::Arg: {
		auto m = std::make_shared<Node>(*n);
		for (Expr& a : m->args)
			a = safeSimplify(a);
		return m;
	}
	default:
		return n;
	}
}

Expr simplify(const Expr& n)
{
	switch (n->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& x : n->args)
			a.push_back(simplify(x));
		return n->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(simplify(n->args[0]), simplify(n->args[1]));
	default:
		return n;
	}
}

// ------------------------------------------------------------- expansion

namespace {

bool needsExpand(const Expr& n)
{
	long k;
	switch (n->kind) {
	case Kind::Pow:
		return n->args[0]->kind == Kind::Add && isInt(n->args[1], k) && k > 1;
	case Kind::Mul:
		for (const Expr& f : n->args)
			if (f->kind == Kind::Add || needsExpand(f))
				return true;
		return false;
	case Kind::Add:
		for (const Expr& t : n->args)
			if (needsExpand(t))
				return true;
		return false;
	default:
		return false;
	}
}

const Exprs& termsOf(const Expr& e, Exprs& single)
{
	if (e->kind == Kind::Add)
		return e->args;
	single = {e};
	return single;
}

Expr mulExpand(const Expr& a, const Expr& b)
{
	Exprs sa, sb, out;
	const Exprs& ta = termsOf(a, sa);
	const Exprs& tb = termsOf(b, sb);
	for (const Expr& x : ta)
		for (const Expr& y : tb) {
			Expr p = mul(x, y);
			out.push_back(needsExpand(p) ? expand(p) : p);
		}
	return simpAdd(out);
}

} // namespace

Expr expand(const Expr& n)
{
	checkComputable(n);
	switch (n->kind) {
	case Kind::Add: {
		Exprs t;
		for (const Expr& x : n->args)
			t.push_back(expand(x));
		return simpAdd(t);
	}
	case Kind::Mul: {
		Expr acc = mkInt(1);
		for (const Expr& f : n->args)
			acc = mulExpand(acc, expand(f));
		return acc;
	}
	case Kind::Pow: {
		Expr b = expand(n->args[0]);
		Expr e = expand(n->args[1]);
		long k;
		if (b->kind == Kind::Add && isInt(e, k) && k > 1) {
			Expr result = mkInt(1), sq = b; // square and multiply
			while (k > 0) {
				if (k & 1)
					result = mulExpand(result, sq);
				k >>= 1;
				if (k)
					sq = mulExpand(sq, sq);
			}
			return result;
		}
		Expr r = simpPow(b, e);
		return needsExpand(r) ? expand(r) : r;
	}
	default:
		return n;
	}
}

// ------------------------------------------------------- differentiation

Expr diff(const Expr& n, const std::string& x)
{
	checkComputable(n);
	switch (n->kind) {
	case Kind::Num:
		return mkInt(0);
	case Kind::Sym:
		return mkInt(n->name == x ? 1 : 0);
	case Kind::Add: {
		Exprs t;
		for (const Expr& a : n->args)
			t.push_back(diff(a, x));
		return simpAdd(t);
	}
	case Kind::Mul: { // product rule
		Exprs sum;
		for (size_t i = 0; i < n->args.size(); i++) {
			Expr d = diff(n->args[i], x);
			if (isZero(d))
				continue;
			Exprs f = n->args;
			f[i] = d;
			sum.push_back(simpMul(f));
		}
		return simpAdd(sum);
	}
	case Kind::Pow: {
		const Expr& b = n->args[0];
		const Expr& e = n->args[1];
		if (containsSym(e, x))
			throw Error("diff: cannot differentiate a power with '" + x +
			            "' in the exponent (logarithms are not supported)");
		return simpMul({e, simpPow(b, sub(e, mkInt(1))), diff(b, x)});
	}
	default:
		throw Error("diff: unsupported expression");
	}
}

// ----------------------------------------------------------- polynomials

namespace {

// degree k -> coefficient of x^k (sparse: x^1000000 is cheap)
using Coeffs = std::map<long, Expr>;

Coeffs collect(const Expr& n, const std::string& x)
{
	Expr e = expand(n);
	Exprs single;
	std::map<long, Exprs> buckets;
	for (const Expr& t : termsOf(e, single)) {
		Exprs fs = t->kind == Kind::Mul ? t->args : Exprs{t};
		long k = 0;
		Exprs rest;
		for (const Expr& f : fs) {
			auto [b, ex] = baseExp(f);
			if (b->kind == Kind::Sym && b->name == x) {
				if (!ex)
					k = 1;
				else if (!isInt(ex, k) || k < 0)
					throw Error("not a polynomial in " + x);
			} else {
				if (containsSym(f, x))
					throw Error("not a polynomial in " + x);
				rest.push_back(f);
			}
		}
		buckets[k].push_back(simpMul(rest));
	}
	Coeffs coeffs;
	for (const auto& [k, terms] : buckets) {
		Expr c = simpAdd(terms);
		if (!isZero(c))
			coeffs.emplace(k, c);
	}
	return coeffs;
}

long degreeOf(const Coeffs& c) { return c.empty() ? -1 : c.rbegin()->first; }

} // namespace

long polyDegree(const Expr& e, const std::string& x) { return degreeOf(collect(e, x)); }

std::map<long, Expr> polyCoefficients(const Expr& e, const std::string& x)
{
	return collect(e, x);
}

bool hasVariableDenominator(const Expr& e) { return hasDenominator(e); }

Expr polyCoeff(const Expr& e, const std::string& x, long k)
{
	Coeffs c = collect(e, x);
	auto it = c.find(k);
	return it == c.end() ? mkInt(0) : it->second;
}

std::pair<Expr, Expr> polyDivide(const Expr& p, const Expr& q, const std::string& x)
{
	Coeffs qc = collect(q, x);
	long dq = degreeOf(qc);
	if (dq < 0)
		throw Error("division by zero polynomial");
	Expr lcInv = simpPow(qc.at(dq), mkInt(-1));
	Expr qe = expand(q);
	Expr r = expand(p);
	Expr quotient = mkInt(0);
	Expr X = mkSym(x);
	for (;;) {
		Coeffs rc = collect(r, x);
		long dr = degreeOf(rc);
		if (dr < dq)
			break;
		Expr t = expand(mul(rc.at(dr), lcInv));
		Expr term = mul(t, simpPow(X, mkInt(dr - dq)));
		quotient = add(quotient, term);
		r = expand(sub(r, expand(mul(term, qe))));
		if (polyDegree(r, x) >= dr)
			throw Error("polynomial division in " + x +
			            " did not terminate (leading coefficient does not cancel)");
	}
	return {expand(quotient), r};
}

} // namespace xyz
