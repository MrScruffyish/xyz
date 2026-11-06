// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Polynomials in one symbol with coefficients a + b*im (a, b rational):
// gcd and factoring over Q(i), the rationals with im added. Factoring uses
// Trager's method: f(x + s*im) times its conjugate is a polynomial with
// rational coefficients (the norm); with s chosen so that the norm is
// square-free, its factors over Q give the factors of f as gcds over Q(i).
#include "expr.hpp"

#include <functional>
#include <map>
#include <set>

namespace xyz {

namespace {

struct C {
	Rational re, im;
	bool isZero() const { return re.isZero() && im.isZero(); }
};

C operator+(const C& a, const C& b) { return {a.re + b.re, a.im + b.im}; }
C operator-(const C& a, const C& b) { return {a.re - b.re, a.im - b.im}; }
C operator*(const C& a, const C& b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }
C operator/(const C& a, const C& b)
{
	Rational d = b.re * b.re + b.im * b.im;
	return {(a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d};
}
C conjOf(const C& a) { return {a.re, -a.im}; }

using GPoly = std::vector<C>; // c[i]: coefficient of x^i

void trim(GPoly& p)
{
	while (!p.empty() && p.back().isZero())
		p.pop_back();
}

long deg(const GPoly& p) { return static_cast<long>(p.size()) - 1; }

GPoly mulP(const GPoly& a, const GPoly& b)
{
	if (a.empty() || b.empty())
		return {};
	GPoly r(a.size() + b.size() - 1, C{});
	for (size_t i = 0; i < a.size(); i++)
		for (size_t j = 0; j < b.size(); j++)
			r[i + j] = r[i + j] + a[i] * b[j];
	trim(r);
	return r;
}

GPoly subP(const GPoly& a, const GPoly& b)
{
	GPoly r(std::max(a.size(), b.size()), C{});
	for (size_t i = 0; i < r.size(); i++)
		r[i] = (i < a.size() ? a[i] : C{}) - (i < b.size() ? b[i] : C{});
	trim(r);
	return r;
}

void divmodP(const GPoly& a, const GPoly& b, GPoly& q, GPoly& r)
{
	r = a;
	q.assign(a.size() >= b.size() ? a.size() - b.size() + 1 : 0, C{});
	while (deg(r) >= deg(b)) {
		size_t k = r.size() - b.size();
		C c = r.back() / b.back();
		q[k] = c;
		for (size_t i = 0; i < b.size(); i++)
			r[i + k] = r[i + k] - c * b[i];
		r.pop_back();
		trim(r);
	}
	trim(q);
}

GPoly quoP(const GPoly& a, const GPoly& b)
{
	GPoly q, r;
	divmodP(a, b, q, r);
	return q;
}

GPoly monic(GPoly p)
{
	if (p.empty())
		return p;
	C l = p.back();
	for (C& c : p)
		c = c / l;
	return p;
}

GPoly gcdP(GPoly a, GPoly b)
{
	while (!b.empty()) {
		GPoly q, r;
		divmodP(a, b, q, r);
		a = std::move(b);
		b = std::move(r);
	}
	return monic(a);
}

GPoly derivP(const GPoly& p)
{
	GPoly r;
	for (size_t i = 1; i < p.size(); i++)
		r.push_back(p[i] * C{Rational(static_cast<long>(i)), Rational(0)});
	trim(r);
	return r;
}

// p(x + c)
GPoly shift(const GPoly& p, const C& c)
{
	GPoly r;
	for (size_t i = p.size(); i-- > 0;) { // r = r*(x + c) + p[i]
		GPoly t = mulP(r, {c, C{Rational(1), Rational(0)}});
		if (t.empty())
			t.push_back(C{});
		t[0] = t[0] + p[i];
		trim(t);
		r = t;
	}
	return r;
}

GPoly conjP(GPoly p)
{
	for (C& c : p)
		c = conjOf(c);
	return p;
}

// the only symbol of e besides im, and its coefficients as numbers a+b*im
bool toGPoly(const Expr& e, std::string& x, GPoly& out)
{
	std::set<std::string> vs;
	std::function<void(const Expr&)> walk = [&](const Expr& n) {
		if (n->kind == Kind::Sym && !isImUnit(n))
			vs.insert(n->name);
		for (const Expr& a : n->args)
			walk(a);
	};
	walk(e);
	if (vs.size() > 1 || (!x.empty() && !vs.empty() && *vs.begin() != x))
		return false;
	if (x.empty() && !vs.empty())
		x = *vs.begin();
	out.clear();
	if (x.empty()) {
		C c;
		if (!complexParts(e, c.re, c.im))
			return false;
		out.push_back(c);
		trim(out);
		return true;
	}
	std::map<long, Expr> cm;
	try {
		cm = polyCoefficients(e, x);
	} catch (const Error&) {
		return false;
	}
	for (const auto& [k, c] : cm) {
		C v;
		if (!complexParts(c, v.re, v.im))
			return false;
		if (out.size() <= static_cast<size_t>(k))
			out.resize(static_cast<size_t>(k) + 1, C{});
		out[static_cast<size_t>(k)] = v;
	}
	trim(out);
	return true;
}

Expr fromC(const C& c) { return makeComplex(c.re, c.im); }

Expr fromGPoly(const GPoly& p, const std::string& x)
{
	Exprs t;
	for (size_t i = 0; i < p.size(); i++)
		if (!p[i].isZero())
			t.push_back(mul(fromC(p[i]), simpPow(mkSym(x), mkInt(static_cast<long>(i)))));
	return simpAdd(t);
}

// square-free parts with multiplicities (Yun, over Q(i))
std::vector<std::pair<GPoly, long>> squareFreeG(const GPoly& f)
{
	std::vector<std::pair<GPoly, long>> out;
	GPoly fp = derivP(f);
	GPoly a = gcdP(f, fp);
	GPoly b = quoP(f, a), c = quoP(fp, a), d = subP(c, derivP(b));
	for (long i = 1; deg(b) > 0; i++) {
		a = gcdP(b, d);
		if (deg(a) > 0)
			out.emplace_back(a, i);
		b = quoP(b, a);
		c = quoP(d, a);
		d = subP(c, derivP(b));
	}
	return out;
}

// the irreducible factors over Q(i) of the square-free, monic f (Trager)
std::vector<GPoly> splitTrager(const GPoly& f, const std::string& x)
{
	if (deg(f) <= 1)
		return {f};
	for (long s = 0; s < 20; s++) {
		C sh{Rational(0), Rational(s)};
		GPoly g = shift(f, sh); // f(x + s*im)
		GPoly n = mulP(g, conjP(g));
		if (deg(gcdP(n, derivP(n))) > 0)
			continue; // the norm is not square-free: another shift
		// n has rational coefficients: factor it over Q
		Expr ne;
		{
			Exprs t;
			for (size_t i = 0; i < n.size(); i++)
				t.push_back(mul(mkNum(n[i].re), simpPow(mkSym(x), mkInt(static_cast<long>(i)))));
			ne = simpAdd(t);
		}
		std::vector<GPoly> out;
		for (const auto& [nf, m] : factorPolynomial(ne).parts) {
			std::string xx = x;
			GPoly np;
			if (!toGPoly(nf, xx, np) || deg(np) < 1)
				continue;
			GPoly h = gcdP(g, np);
			if (deg(h) > 0)
				out.push_back(monic(shift(h, C{Rational(0), Rational(-s)}))); // back: x - s*im
		}
		return out;
	}
	return {f};
}

} // namespace

bool gaussianGcd(const Expr& a, const Expr& b, Expr& result)
{
	if (!containsSym(a, kImag) && !containsSym(b, kImag))
		return false;
	std::string x;
	GPoly pa, pb;
	if (!toGPoly(a, x, pa) || !toGPoly(b, x, pb))
		return false;
	if (pa.empty() && pb.empty())
		return false;
	GPoly g = gcdP(pa, pb);
	result = x.empty() || g.size() <= 1 ? mkInt(1) : fromGPoly(g, x);
	return true;
}

bool gaussianQuotient(const Expr& a, const Expr& b, Expr& q)
{
	std::string x;
	GPoly pa, pb;
	if (!toGPoly(a, x, pa) || !toGPoly(b, x, pb) || pb.empty())
		return false;
	GPoly qq, r;
	divmodP(pa, pb, qq, r);
	if (!r.empty())
		return false;
	q = x.empty() ? (qq.empty() ? mkInt(0) : fromC(qq[0])) : fromGPoly(qq, x);
	return true;
}

bool gaussianFactor(const Expr& p, Expr& content, std::vector<std::pair<Expr, long>>& parts)
{
	std::string x;
	GPoly f;
	if (!toGPoly(p, x, f) || x.empty() || deg(f) < 1)
		return false;
	content = fromC(f.back());
	parts.clear();
	for (const auto& [s, m] : squareFreeG(monic(f)))
		for (const GPoly& h : splitTrager(s, x))
			parts.emplace_back(fromGPoly(h, x), m);
	return true;
}

} // namespace xyz
