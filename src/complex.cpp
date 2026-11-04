// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Real and imaginary parts, conjugate and absolute value.
//
// An expression is split into re + im*Im where that can be done: numbers,
// im, symbols (taken to be real), sums, products, integer powers
// (1/(a+b*im) = (a-b*im)/(a^2+b^2)), powers of positive numbers with real
// exponents, and Re/Im/abs/conj themselves. Anything else, such as
// x^(1/2) (x could be negative), cannot be split, and the function is
// returned as it is: Re(x^(1/2)).
#include "expr.hpp"

#include <optional>

namespace xyz {

namespace {

struct Parts {
	Expr re, im;
};

bool knownZero(const Expr& e) { return isZero(expand(e)); }

Parts times(const Parts& a, const Parts& b)
{
	return {sub(mul(a.re, b.re), mul(a.im, b.im)), add(mul(a.re, b.im), mul(a.im, b.re))};
}

std::optional<Parts> split(const Expr& e)
{
	switch (e->kind) {
	case Kind::Num:
		return Parts{e, mkInt(0)};
	case Kind::Sym:
		if (isImUnit(e))
			return Parts{mkInt(0), mkInt(1)};
		return Parts{e, mkInt(0)}; // symbols are real
	case Kind::Add: {
		Parts r{mkInt(0), mkInt(0)};
		for (const Expr& t : e->args) {
			auto p = split(t);
			if (!p)
				return std::nullopt;
			r = {add(r.re, p->re), add(r.im, p->im)};
		}
		return r;
	}
	case Kind::Mul: {
		Parts r{mkInt(1), mkInt(0)};
		for (const Expr& f : e->args) {
			auto p = split(f);
			if (!p)
				return std::nullopt;
			r = times(r, *p);
		}
		return r;
	}
	case Kind::Pow: {
		const Expr& b = e->args[0];
		const Expr& x = e->args[1];
		long k;
		if (isInt(x, k)) {
			auto p = split(b);
			if (!p)
				return std::nullopt;
			if (knownZero(p->im))
				return Parts{e, mkInt(0)}; // a real number to an integer power
			Parts base = *p;
			if (k < 0) { // 1/(a+b*im) = (a-b*im)/(a^2+b^2)
				Expr d = add(mul(base.re, base.re), mul(base.im, base.im));
				Expr inv = simpPow(d, mkInt(-1));
				base = {mul(base.re, inv), mul(mkInt(-1), mul(base.im, inv))};
				k = -k;
			}
			Parts r{mkInt(1), mkInt(0)};
			for (long i = 0; i < k; i++)
				r = times(r, base);
			return r;
		}
		// a positive number to a real power is real: 2^(1/2), 2^x
		if (b->kind == Kind::Num && b->num.sign() > 0) {
			auto px = split(x);
			if (px && knownZero(px->im))
				return Parts{e, mkInt(0)};
		}
		return std::nullopt;
	}
	case Kind::Call:
		if (e->name == "Re" || e->name == "Im" || e->name == "abs")
			return Parts{e, mkInt(0)};
		if (e->name == "conj") {
			auto p = split(e->args[0]);
			if (p)
				return Parts{p->re, mul(mkInt(-1), p->im)};
		}
		return std::nullopt;
	default:
		return std::nullopt;
	}
}

// tidies a part: one fraction if it divides by something with symbols,
// otherwise the automatic simplification is enough
Expr tidy(const Expr& e)
{
	return hasVariableDenominator(e) ? cancel(e) : e;
}

} // namespace

Expr realPart(const Expr& e)
{
	auto p = split(e);
	return p ? tidy(p->re) : mkCall("Re", {e});
}

Expr imagPart(const Expr& e)
{
	auto p = split(e);
	return p ? tidy(p->im) : mkCall("Im", {e});
}

Expr conjugate(const Expr& e)
{
	if (e->kind == Kind::Call && e->name == "conj")
		return e->args[0]; // conj(conj(z)) = z
	auto p = split(e);
	if (!p)
		return mkCall("conj", {e});
	return add(tidy(p->re), mul(mkInt(-1), mul(mkIm(), tidy(p->im))));
}

Expr absValue(const Expr& e)
{
	auto p = split(e);
	if (!p)
		return mkCall("abs", {e});
	Expr re = tidy(p->re);
	if (knownZero(p->im)) {
		if (re->kind == Kind::Num)
			return mkNum(re->num.abs());
		// abs(-3*x) = 3*abs(x)
		if (re->kind == Kind::Mul && re->args[0]->kind == Kind::Num) {
			Exprs rest(re->args.begin() + 1, re->args.end());
			Expr r = rest.size() == 1 ? rest[0] : mkNode(Kind::Mul, rest);
			return mul(mkNum(re->args[0]->num.abs()), mkCall("abs", {r}));
		}
		return mkCall("abs", {re});
	}
	Expr im = tidy(p->im);
	if (knownZero(re) && im->kind == Kind::Num)
		return mkNum(im->num.abs());
	// sqrt(re^2 + im^2)
	Expr n = expand(add(mul(re, re), mul(im, im)));
	return simpPow(tidy(n), mkNum(Rational(1) / Rational(2)));
}

} // namespace xyz
