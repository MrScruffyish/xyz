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
	if (e->kind == Kind::Str)
		throw Error("strings can only be used as file names in save() and "
		            "transcript()");
	if (isList(e))
		throw Error("lists can only be added, subtracted, multiplied, divided and "
		            "raised to powers element by element, or used with functions");
	if (e->kind == Kind::Held)
		throw Error(e->args.size() < 2 ? e->name : "a factor() result reached the simplifier");
	if (e->kind == Kind::Call && isSymbolicFunction(e->name))
		return; // e.g. Re(x^(1/2)): an atom
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

// the factors of a term without its numeric coefficient; im counts as part
// of the coefficient, so x^2+2*im*x-1 is ordered like x^2+2*x-1
Exprs termFactors(const Expr& t)
{
	Exprs f;
	if (t->kind == Kind::Mul) {
		for (const Expr& x : t->args)
			if (x->kind != Kind::Num && !isImUnit(x))
				f.push_back(x);
	} else if (t->kind != Kind::Num && !isImUnit(t)) {
		f.push_back(t);
	}
	return f;
}

bool hasImFactor(const Expr& t)
{
	if (isImUnit(t))
		return true;
	if (t->kind == Kind::Mul)
		for (const Expr& x : t->args)
			if (isImUnit(x))
				return true;
	return false;
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
	if (hasImFactor(a) != hasImFactor(b)) // x before im*x
		return !hasImFactor(a);
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

bool isSymbolicFunction(const std::string& name)
{
	return name == "Re" || name == "Im" || name == "conj" || name == "abs" ||
	       name == "integrate" || isElementary(name);
}

bool complexParts(const Expr& e, Rational& re, Rational& im)
{
	switch (e->kind) {
	case Kind::Num:
		re = e->num;
		im = Rational(0);
		return true;
	case Kind::Sym:
		if (!isImUnit(e))
			return false;
		re = Rational(0);
		im = Rational(1);
		return true;
	case Kind::Mul:
		if (e->args.size() == 2 && e->args[0]->kind == Kind::Num && isImUnit(e->args[1])) {
			re = Rational(0);
			im = e->args[0]->num;
			return true;
		}
		return false;
	case Kind::Add: {
		Rational r, i;
		for (const Expr& t : e->args) {
			Rational a, b;
			if (!complexParts(t, a, b))
				return false;
			r = r + a;
			i = i + b;
		}
		re = r;
		im = i;
		return true;
	}
	default:
		return false;
	}
}

Expr makeComplex(const Rational& re, const Rational& im)
{
	return add(mkNum(re), mul(mkNum(im), mkIm()));
}

namespace {

// symbols other than im and pi?
bool hasVariable(const Expr& e)
{
	if (e->kind == Kind::Sym)
		return !isImUnit(e) && e->name != "pi";
	for (const Expr& a : e->args)
		if (hasVariable(a))
			return true;
	return false;
}

// the imaginary part of a complex number: b*im without variables
// (3^(1/2)*im/2), so that it is written last: 1/2+3^(1/2)*im/2
bool isImTerm(const Expr& t)
{
	if (isImUnit(t))
		return true;
	if (t->kind != Kind::Mul || hasVariable(t))
		return false;
	for (const Expr& f : t->args)
		if (isImUnit(f))
			return true;
	return false;
}

// n = s^2 * t with t as small as we can find cheaply (small primes and a
// final perfect-square test; factoring large numbers is not attempted)
void squareFactor(const BigInt& n, BigInt& s, BigInt& t)
{
	s = BigInt(1L);
	t = n;
	BigInt r = t.isqrt();
	if (r * r == t) {
		s = r;
		t = BigInt(1L);
		return;
	}
	for (long p = 2; p < 1000; p++) {
		BigInt pp(p * p);
		if (pp.compare(t) > 0)
			break;
		while (t.mod(pp).isZero()) {
			t = t.divTrunc(pp);
			s = s * BigInt(p);
		}
	}
	r = t.isqrt();
	if (r * r == t) {
		s = s * r;
		t = BigInt(1L);
	}
}

// v^(sgn/2) for a positive rational v and sgn = +-1:
// sqrt(p/q) = sqrt(p*q)/q = c*sqrt(t), with t as small as found
Expr rootPow(const Rational& v, long sgn)
{
	BigInt s, t;
	squareFactor(v.numerator() * v.denominator(), s, t);
	Rational c(s, v.denominator());
	Rational tr(t, BigInt(1L));
	if (sgn < 0) // 1/(c*sqrt(t)) = sqrt(t)/(c*t): roots stay in the numerator
		c = Rational(1) / (t.isOne() ? c : c * tr);
	if (t.isOne())
		return mkNum(c);
	// built directly (not simplified again, which would come back here)
	Expr r = mkPow(mkNum(tr), mkNum(Rational(1) / Rational(2)));
	return c.isOne() ? r : mkNode(Kind::Mul, {mkNum(c), r});
}

// (re + im*i)^k for an exact complex number
Expr complexPow(Rational re, Rational im, long k)
{
	if (k < 0) {
		Rational d = re * re + im * im;
		if (d.isZero())
			throw Error("division by zero");
		re = re / d;
		im = -im / d;
		k = -k;
	}
	Rational rr(1), ri(0);
	while (k > 0) {
		if (k & 1) {
			Rational a = rr * re - ri * im;
			ri = rr * im + ri * re;
			rr = a;
		}
		Rational a = re * re - im * im;
		im = Rational(2) * re * im;
		re = a;
		k >>= 1;
	}
	return makeComplex(rr, ri);
}

} // namespace

Expr add(const Expr& a, const Expr& b) { return simpAdd({a, b}); }
Expr mul(const Expr& a, const Expr& b) { return simpMul({a, b}); }
Expr sub(const Expr& a, const Expr& b) { return add(a, mul(mkInt(-1), b)); }

// ------------------------------------------------------------------ sums

namespace {

// c*R*sin(u)^2 + c*R*cos(u)^2 -> c*R  (R: the other factors, maybe none);
// with a = cosh, b = sinh, sign = -1: c*R*cosh(u)^2 - c*R*sinh(u)^2 -> c*R
template <class Terms>
void sinCosSquares(Terms& terms, Rational& konst, const char* a = "sin", const char* b = "cos",
                   int sign = 1)
{
	auto squareOf = [](const Expr& f, const char* name) {
		return f->kind == Kind::Pow && isNumber(f->args[1], 2) &&
		       f->args[0]->kind == Kind::Call && f->args[0]->name == name;
	};
	// replaces one pair; false when there is none left
	auto onePair = [&]() {
		for (auto it = terms.begin(); it != terms.end(); ++it) {
			const Expr rest = it->first;
			Exprs fs = rest->kind == Kind::Mul ? rest->args : Exprs{rest};
			for (size_t i = 0; i < fs.size(); i++) {
				if (!squareOf(fs[i], a))
					continue;
				Exprs other = fs;
				other[i] = mkPow(mkCall(b, fs[i]->args[0]->args), mkInt(2));
				auto jt = terms.find(simpMul(other));
				if (jt == terms.end() || jt->second.compare(it->second * Rational(sign)) != 0)
					continue;
				Rational c = it->second;
				Exprs remaining = fs;
				remaining.erase(remaining.begin() + static_cast<long>(i));
				terms.erase(jt);
				terms.erase(it); // not used after this
				if (remaining.empty()) {
					konst = konst + c;
				} else {
					Expr r = simpMul(remaining);
					auto kt = terms.find(r);
					if (kt == terms.end())
						terms.emplace(r, c);
					else
						kt->second = kt->second + c;
				}
				return true;
			}
		}
		return false;
	};
	while (onePair()) {
	}
}

} // namespace

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
	sinCosSquares(terms, konst);
	sinCosSquares(terms, konst, "cosh", "sinh", -1);
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
	// a complex number reads a+b*im: the im terms go after the constant
	Exprs imTerms, rest;
	for (const Expr& t : out)
		(isImTerm(t) ? imTerms : rest).push_back(t);
	out = rest;
	if (!konst.isZero())
		out.push_back(mkNum(konst));
	out.insert(out.end(), imTerms.begin(), imTerms.end());
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
	{
		// exact complex numbers are multiplied out: (3+im)*(5-3im) -> 18-4*im
		Rational re(1), imv(0);
		Exprs others;
		int count = 0;
		bool hasSum = false;
		for (const Expr& f : out) {
			Rational a, b;
			if ((f->kind == Kind::Add || isImUnit(f)) && complexParts(f, a, b)) {
				count++;
				hasSum = hasSum || f->kind == Kind::Add;
				Rational nr = re * a - imv * b;
				imv = re * b + imv * a;
				re = nr;
			} else {
				others.push_back(f);
			}
		}
		if (hasSum && (count > 1 || others.empty())) {
			if (others.empty())
				return makeComplex(coeff * re, coeff * imv);
			if (imv.isZero()) {
				coeff = coeff * re;
			} else if (re.isZero()) {
				coeff = coeff * imv;
				others.push_back(mkIm());
			} else {
				others.push_back(makeComplex(re, imv));
			}
			out = others;
		}
	}
	{
		// exp(a)*exp(b) -> exp(a+b)
		Exprs args, rest;
		for (const Expr& f : out) {
			if (f->kind == Kind::Call && f->name == "exp")
				args.push_back(f->args[0]);
			else
				rest.push_back(f);
		}
		if (args.size() > 1) {
			Expr e = makeFunction("exp", {simpAdd(args)});
			rest.push_back(mkNum(coeff));
			rest.push_back(e);
			return simpMul(rest);
		}
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
		if (isImUnit(b) && eint && fits) { // im^2 = -1
			switch (((k % 4) + 4) % 4) {
			case 0: return mkInt(1);
			case 1: return b;
			case 2: return mkInt(-1);
			default: return mul(mkInt(-1), b);
			}
		}
		Rational cr, ci;
		if (b->kind == Kind::Add && eint && complexParts(b, cr, ci)) {
			if (!fits)
				throw Error("exponent too large");
			return complexPow(cr, ci, k);
		}
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
			if (e->num.denominator() == BigInt(2L)) {
				// square roots: b^(m/2) = sqrt(b)^m, exact where possible,
				// sqrt(-4) = 2*im (principal value)
				long m;
				if (!e->num.numerator().toLong(m))
					throw Error("exponent too large");
				if (m == 1 || m == -1) {
					if (b->num.sign() > 0)
						return rootPow(b->num, m);
					// 1/im = -im
					return mul(simpPow(mkIm(), mkInt(m)), rootPow(-b->num, m));
				}
				// b^(m/2) = b^((m-1)/2) * b^(1/2)
				long sgn = m > 0 ? 1 : -1;
				return mul(simpPow(b, mkInt((m - sgn) / 2)),
				           simpPow(b, mkNum(Rational(sgn) / Rational(2))));
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
		if (eint && b->kind == Kind::Call && b->name == "exp") // exp(u)^k = exp(k*u)
			return makeFunction("exp", {mul(e, b->args[0])});
		// (a^k)^e = a^(k*e) when a >= 0 (assumed): sqrt(x^2) = x for x > 0
		if (b->kind == Kind::Pow && b->args[1]->kind == Kind::Num && nonNegative(b->args[0]))
			return simpPow(b->args[0], mul(b->args[1], e));
		// abs(u)^(2k) = u^(2k) for real u
		if (eint && b->kind == Kind::Call && b->name == "abs" && b->args.size() == 1 &&
		    !containsSym(b->args[0], kImag) && e->num.numerator().mod(BigInt(2L)).isZero())
			return simpPow(b->args[0], e);
		// sqrt(s^(2k)) = abs(s)^k for a real symbol s
		if (e->num.compare(Rational(1) / Rational(2)) == 0 && b->kind == Kind::Pow &&
		    b->args[0]->kind == Kind::Sym && !isImUnit(b->args[0])) {
			long k2;
			if (isInt(b->args[1], k2) && k2 > 0 && k2 % 2 == 0)
				return simpPow(absValue(b->args[0]), mkInt(k2 / 2));
		}

		if (eint) {
			// (a^k)^n -> a^(k*n) only for integer k: (x^(1/2))^2 is
			// not simplified to x (roots are decided later)
			// ... and for positive numbers: (2^(1/2))^2 -> 2
			if (b->kind == Kind::Pow && b->args[1]->kind == Kind::Num &&
			    (b->args[1]->num.isInt() ||
			     (b->args[0]->kind == Kind::Num && b->args[0]->num.sign() > 0)))
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
	if (isNumber(b, -1) && e->kind == Kind::Mul && e->args[0]->kind == Kind::Num &&
	    e->args[0]->num.isInt()) { // (-1)^(c*m) with integer m: by the parity of c
		Exprs rest(e->args.begin() + 1, e->args.end());
		Expr m = simpMul(rest);
		if (isIntegerValued(m)) {
			BigInt c = e->args[0]->num.numerator().abs();
			if (c.mod(BigInt(2L)).isZero())
				return mkInt(1);
			return mkPow(b, m);
		}
	}
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
		if (containsSym(e, x)) // d(b^e) = b^e * (e'*ln(b) + e*b'/b)
			return mul(n, add(mul(diff(e, x), makeFunction("ln", {b})),
			                  mul(e, mul(diff(b, x), simpPow(b, mkInt(-1))))));
		return simpMul({e, simpPow(b, sub(e, mkInt(1))), diff(b, x)});
	}
	case Kind::Call: {
		bool dependsOnX = false;
		for (const Expr& a : n->args)
			dependsOnX = dependsOnX || containsSym(a, x);
		if (!dependsOnX)
			return mkInt(0);
		if (n->name == "integrate" && n->args.size() == 2 && n->args[1]->kind == Kind::Sym &&
		    n->args[1]->name == x)
			return n->args[0]; // d/dx of an integral in x
		if (!isElementary(n->name))
			throw Error("diff: cannot differentiate " + n->name + "()");
		const Expr& u = n->args[0];
		return mul(derivativeOf(n->name, u), diff(u, x)); // chain rule
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
