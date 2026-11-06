// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// integrate(): antiderivatives by a set of methods, tried in turn (not the
// Risch algorithm, so some integrals that exist are not found):
//  - linearity, and a table (powers, exp, ln, sin, cos, tan, the inverse and
//    hyperbolic functions, 1/cos^2, 1/sin, ...)
//  - rational functions: polynomial division and partial fractions over the
//    factors found by factor() (linear and quadratic factors)
//  - sqrt of a quadratic and 1/sqrt of a quadratic
//  - integration by parts: polynomial * exp/sin/cos/sinh/cosh,
//    polynomial * ln/asin/acos/atan, and exp * sin/cos
//  - substitution: f(u(x))*u'(x)
//  - products and powers of sin and cos (with trigreduce)
// Logarithms are written without absolute values: ln(x), not ln|x|.
#include "expr.hpp"

#include <functional>
#include <map>
#include <optional>
#include <set>

namespace xyz {

namespace {

using Opt = std::optional<Expr>;

const int kMaxDepth = 6;

Expr num(const Rational& r) { return mkNum(r); }
Expr neg(const Expr& e) { return mul(mkInt(-1), e); }
Expr dv(const Expr& a, const Expr& b) { return mul(a, simpPow(b, mkInt(-1))); }
Expr half() { return mkNum(Rational(1) / Rational(2)); }
Expr fn(const char* name, const Expr& u) { return makeFunction(name, {u}); }
bool isCall(const Expr& e, const char* n) { return e->kind == Kind::Call && e->name == n; }

// u = a*x + b with a, b free of x (a != 0)?
bool linearIn(const Expr& u, const std::string& x, Expr& a, Expr& b)
{
	if (!containsSym(u, x))
		return false;
	std::map<long, Expr> cm;
	try {
		cm = polyCoefficients(u, x);
	} catch (const Error&) {
		return false;
	}
	if (cm.rbegin()->first != 1)
		return false;
	for (const auto& [k, c] : cm)
		if (containsSym(c, x))
			return false;
	a = cm[1];
	b = cm.count(0) ? cm[0] : mkInt(0);
	return true;
}

// c0 + c1*x + c2*x^2 with numbers? (c2 != 0)
bool quadraticIn(const Expr& u, const std::string& x, Rational& c2, Rational& c1, Rational& c0)
{
	std::map<long, Expr> cm;
	try {
		cm = polyCoefficients(u, x);
	} catch (const Error&) {
		return false;
	}
	if (cm.empty() || cm.rbegin()->first != 2)
		return false;
	for (const auto& [k, c] : cm)
		if (c->kind != Kind::Num)
			return false;
	auto get = [&](long k) { return cm.count(k) ? cm[k]->num : Rational(0); };
	c2 = get(2);
	c1 = get(1);
	c0 = get(0);
	return true;
}

// e with every occurrence of the subexpression u replaced by t
Expr replaceAll(const Expr& e, const Expr& u, const Expr& t)
{
	if (equal(e, u))
		return t;
	// exp(k*v) = t^k for u = exp(v), and b^q = t^(q/p) for u = b^p (integer ratio)
	if (isCall(u, "exp") && isCall(e, "exp")) {
		Expr k = cancel(dv(e->args[0], u->args[0]));
		long n;
		if (isInt(k, n))
			return simpPow(t, k);
	}
	if (u->kind == Kind::Pow && e->kind == Kind::Pow && equal(u->args[0], e->args[0]) &&
	    u->args[1]->kind == Kind::Num && e->args[1]->kind == Kind::Num) {
		Rational k = e->args[1]->num / u->args[1]->num;
		if (k.isInt())
			return simpPow(t, num(k));
	}
	if (u->kind == Kind::Pow && u->args[1]->kind == Kind::Num && equal(e, u->args[0]) &&
	    (Rational(1) / u->args[1]->num).isInt())
		return simpPow(t, num(Rational(1) / u->args[1]->num)); // x = (x^(1/2))^2
	switch (e->kind) {
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& c : e->args)
			a.push_back(replaceAll(c, u, t));
		return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(replaceAll(e->args[0], u, t), replaceAll(e->args[1], u, t));
	case Kind::Call: {
		Exprs a;
		for (const Expr& c : e->args)
			a.push_back(replaceAll(c, u, t));
		if (isElementary(e->name))
			return makeFunction(e->name, a);
		return mkCall(e->name, a);
	}
	default:
		return e;
	}
}

// is f a fraction of polynomials in x (other parts free of x)?
bool isRationalIn(const Expr& f, const std::string& x)
{
	long k;
	switch (f->kind) {
	case Kind::Num:
	case Kind::Sym:
		return true;
	case Kind::Add:
	case Kind::Mul:
		for (const Expr& a : f->args)
			if (!isRationalIn(a, x))
				return false;
		return true;
	case Kind::Pow:
		if (!containsSym(f, x))
			return true;
		return isInt(f->args[1], k) && isRationalIn(f->args[0], x);
	default:
		return !containsSym(f, x);
	}
}

Expr exactQuo(const Expr& a, const Expr& b)
{
	auto [n, d] = cancelParts(dv(a, b));
	return expand(dv(n, d));
}

struct Integrator {
	std::string x;
	Expr X;

	explicit Integrator(std::string v) : x(std::move(v)), X(mkSym(x)) {}

	Opt integ(const Expr& f, int depth)
	{
		if (depth > kMaxDepth)
			return std::nullopt;
		if (!containsSym(f, x))
			return mul(f, X);
		if (f->kind == Kind::Add) {
			Exprs parts;
			for (const Expr& t : f->args) {
				Opt r = integ(t, depth);
				if (!r)
					return std::nullopt;
				parts.push_back(*r);
			}
			return simpAdd(parts);
		}
		if (f->kind == Kind::Mul) { // constant factors out
			Exprs c, rest;
			for (const Expr& a : f->args)
				(containsSym(a, x) ? rest : c).push_back(a);
			if (!c.empty()) {
				Opt r = integ(simpMul(rest), depth);
				if (r)
					return mul(simpMul(c), *r);
				return std::nullopt;
			}
		}
		if (Opt r = table(f))
			return r;
		if (isRationalIn(f, x))
			if (Opt r = rational(f, depth))
				return r;
		if (Opt r = roots(f))
			return r;
		if (Opt r = byParts(f, depth))
			return r;
		if (Opt r = substitution(f, depth))
			return r;
		if (Opt r = trig(f, depth))
			return r;
		Expr e = expand(f);
		if (!equal(e, f) && e->kind == Kind::Add)
			return integ(e, depth + 1);
		return std::nullopt;
	}

	// --------------------------------------------------------------- table

	// f(a*x + b) for the functions with a known antiderivative F: F(a*x+b)/a
	Opt table(const Expr& f)
	{
		Expr a, b;
		if (f->kind == Kind::Sym) // x
			return mul(half(), simpPow(X, mkInt(2)));
		if (f->kind == Kind::Pow) {
			const Expr& base = f->args[0];
			const Expr& e = f->args[1];
			if (!containsSym(e, x) && linearIn(base, x, a, b)) { // (a*x+b)^e
				if (isNumber(e, -1))
					return dv(fn("ln", base), a);
				Expr e1 = add(e, mkInt(1));
				return dv(simpPow(base, e1), mul(a, e1));
			}
			if (!containsSym(base, x) && linearIn(e, x, a, b)) // c^(a*x+b)
				return dv(f, mul(a, fn("ln", base)));
			long k;
			if (isInt(e, k) && base->kind == Kind::Call && base->args.size() == 1 &&
			    linearIn(base->args[0], x, a, b)) {
				const Expr& u = base->args[0];
				const std::string& n = base->name;
				if (k == -2 && n == "cos")
					return dv(fn("tan", u), a);
				if (k == -2 && n == "sin")
					return neg(dv(simpPow(fn("tan", u), mkInt(-1)), a));
				if (k == -2 && n == "cosh")
					return dv(fn("tanh", u), a);
				if (k == -1 && n == "cos") // ln((1 + sin u)/cos u)
					return dv(fn("ln", dv(add(mkInt(1), fn("sin", u)), fn("cos", u))), a);
				if (k == -1 && n == "sin") // ln((1 - cos u)/sin u)
					return dv(fn("ln", dv(sub(mkInt(1), fn("cos", u)), fn("sin", u))), a);
				if (k == 2 && n == "tan") // tan^2 = 1/cos^2 - 1
					return sub(dv(fn("tan", u), a), X);
				if (k == 2 && n == "tanh")
					return sub(X, dv(fn("tanh", u), a));
			}
			return std::nullopt;
		}
		if (f->kind != Kind::Call || f->args.size() != 1 || !linearIn(f->args[0], x, a, b))
			return std::nullopt;
		const Expr& u = f->args[0];
		const std::string& n = f->name;
		Expr F;
		Expr one = mkInt(1);
		if (n == "exp") F = f;
		else if (n == "sin") F = neg(fn("cos", u));
		else if (n == "cos") F = fn("sin", u);
		else if (n == "tan") F = neg(fn("ln", fn("cos", u)));
		else if (n == "sinh") F = fn("cosh", u);
		else if (n == "cosh") F = fn("sinh", u);
		else if (n == "tanh") F = fn("ln", fn("cosh", u));
		else if (n == "ln") F = sub(mul(u, f), u);
		else if (n == "abs") F = mul(half(), mul(u, f));
		else if (n == "asin") F = add(mul(u, f), simpPow(sub(one, mul(u, u)), half()));
		else if (n == "acos") F = sub(mul(u, f), simpPow(sub(one, mul(u, u)), half()));
		else if (n == "atan") F = sub(mul(u, f), mul(half(), fn("ln", add(mul(u, u), one))));
		else if (n == "asinh") F = sub(mul(u, f), simpPow(add(mul(u, u), one), half()));
		else if (n == "acosh") F = sub(mul(u, f), simpPow(sub(mul(u, u), one), half()));
		else if (n == "atanh") F = add(mul(u, f), mul(half(), fn("ln", sub(one, mul(u, u)))));
		else return std::nullopt;
		return dv(F, a);
	}

	// --------------------------------------------------- rational functions

	// A/(a*x+b)^j
	Expr linearTerm(const Expr& A, const Expr& l, long j)
	{
		Expr a, b;
		linearIn(l, x, a, b);
		if (j == 1)
			return dv(mul(A, fn("ln", l)), a);
		return dv(mul(A, simpPow(l, mkInt(1 - j))), mul(a, mkInt(1 - j)));
	}

	// 1/Q^j for Q = a*x^2 + b*x + c without real roots (or symbolic)
	Expr quadInverse(const Expr& Q, long j, const Expr& a, const Expr& b, const Expr& c)
	{
		Expr D = expand(sub(mul(mkInt(4), mul(a, c)), mul(b, b))); // 4ac - b^2
		Expr lin = add(mul(mkInt(2), mul(a, X)), b);
		if (j == 1) { // Q = a((x + h)^2 + k)
			Expr h = dv(b, mul(mkInt(2), a));
			Expr k = expand(sub(dv(c, a), mul(h, h)));
			Expr u = add(X, h);
			if (k->kind == Kind::Num && k->num.sign() < 0) { // real roots (irrational)
				Expr s = simpPow(neg(k), half());
				return dv(fn("ln", dv(sub(u, s), add(u, s))), mul(mkInt(2), mul(a, s)));
			}
			Expr s = simpPow(k, half());
			return dv(fn("atan", dv(u, s)), mul(a, s));
		}
		// reduction: 1/Q^j = lin/((j-1) D Q^(j-1)) + 2(2j-3)a/((j-1) D) * 1/Q^(j-1)
		Expr jm = mkInt(j - 1);
		Expr t1 = dv(lin, mul(mul(jm, D), simpPow(Q, jm)));
		Expr t2 = mul(dv(mul(mkInt(2 * (2 * j - 3)), a), mul(jm, D)), quadInverse(Q, j - 1, a, b, c));
		return add(t1, t2);
	}

	// (B*x + C)/Q^j
	Expr quadTerm(const Expr& B, const Expr& C, const Expr& Q, long j)
	{
		auto cm = polyCoefficients(Q, x);
		Expr a = cm[2], b = cm.count(1) ? cm[1] : mkInt(0), c = cm.count(0) ? cm[0] : mkInt(0);
		// B*x + C = B/(2a) * Q' + (C - B*b/(2a))
		Expr k = dv(B, mul(mkInt(2), a));
		Expr first = j == 1 ? fn("ln", Q) : dv(simpPow(Q, mkInt(1 - j)), mkInt(1 - j));
		Expr rest = sub(C, mul(k, b));
		return add(mul(k, first), mul(rest, quadInverse(Q, j, a, b, c)));
	}

	Opt rational(const Expr& f, int depth)
	{
		auto [p, q] = cancelParts(f);
		if (!containsSym(q, x)) { // a polynomial
			auto cm = polyCoefficients(p, x);
			Exprs t;
			for (const auto& [k, c] : cm)
				t.push_back(mul(dv(c, mkInt(k + 1)), simpPow(X, mkInt(k + 1))));
			return dv(simpAdd(t), q);
		}
		auto [s, r] = polyDivide(p, q, x);
		Factored fq = factorPolynomial(q);
		// ansatz: sum over the factors l^m of A_1/l + ... + A_m/l^m (linear l),
		// (B_1 x + C_1)/l + ... (quadratic l); factors without x go to the content
		struct Term {
			Expr l;
			long j;
			Expr A, C; // C only for quadratics
		};
		std::vector<Term> terms;
		std::vector<std::string> unknowns;
		Expr content = num(fq.content);
		auto fresh = [&]() {
			unknowns.push_back("~p" + std::to_string(unknowns.size() + 1));
			return mkSym(unknowns.back());
		};
		Expr qx = mkInt(1); // the part of q with x
		for (const auto& [l, m] : fq.parts) {
			if (!containsSym(l, x)) {
				content = mul(content, simpPow(l, mkInt(m)));
				continue;
			}
			qx = mul(qx, simpPow(l, mkInt(m)));
			long d = polyDegree(l, x);
			if (d > 2)
				return std::nullopt; // would need roots of a cubic or more
			for (long j = 1; j <= m; j++)
				terms.push_back({l, j, fresh(), d == 2 ? fresh() : nullptr});
		}
		// r/content = sum of A * qx / l^j (polynomial identity in x)
		Expr rhs = mkInt(0);
		for (const Term& t : terms) {
			Expr num_ = t.C ? add(mul(t.A, X), t.C) : t.A;
			rhs = add(rhs, mul(num_, exactQuo(qx, simpPow(t.l, mkInt(t.j)))));
		}
		Expr diffPoly = expand(sub(dv(r, content), rhs));
		Exprs eqs;
		if (!isZero(diffPoly))
			for (const auto& [k, c] : polyCoefficients(diffPoly, x))
				eqs.push_back(c);
		LinearSolution ls = solveLinear(eqs, unknowns);
		if (ls.none)
			return std::nullopt;
		auto value = [&](const Expr& sym) {
			for (size_t i = 0; i < unknowns.size(); i++)
				if (unknowns[i] == sym->name)
					return ls.values[i];
			return sym;
		};
		Exprs out;
		if (Opt ps = rational(s, depth))
			out.push_back(*ps);
		for (const Term& t : terms) {
			Expr A = value(t.A);
			if (isZero(A) && (!t.C || isZero(value(t.C))))
				continue;
			out.push_back(t.C ? quadTerm(A, value(t.C), t.l, t.j) : linearTerm(A, t.l, t.j));
		}
		return simpAdd(out);
	}

	// ------------------------------------------- square roots of quadratics

	Opt roots(const Expr& f)
	{
		if (f->kind != Kind::Pow || f->args[1]->kind != Kind::Num)
			return std::nullopt;
		Rational e = f->args[1]->num;
		Expr q = f->args[0];
		if (q->kind == Kind::Pow && q->args[1]->kind == Kind::Num) { // (q^(1/2))^-1
			e = e * q->args[1]->num;
			q = q->args[0];
		}
		bool inv = e.compare(Rational(-1) / Rational(2)) == 0;
		if (!inv && e.compare(Rational(1) / Rational(2)) != 0)
			return std::nullopt;
		Rational a, b, c;
		if (!quadraticIn(q, x, a, b, c))
			return std::nullopt;
		// q = a (x + h)^2 + k
		Rational h = b / (Rational(2) * a), k = c - b * b / (Rational(4) * a);
		Expr u = add(X, num(h));
		Expr invRoot; // integral of q^(-1/2)
		if (a.sign() < 0) {
			if (k.sign() <= 0)
				return std::nullopt; // q < 0 everywhere
			// asin(u*sqrt(-a/k))/sqrt(-a)
			invRoot = dv(fn("asin", mul(u, simpPow(num(-a / k), half()))), simpPow(num(-a), half()));
		} else if (k.isZero()) {
			invRoot = dv(fn("ln", u), simpPow(num(a), half()));
		} else if (k.sign() > 0) {
			invRoot = dv(fn("asinh", mul(u, simpPow(num(a / k), half()))), simpPow(num(a), half()));
		} else {
			invRoot = dv(fn("acosh", mul(u, simpPow(num(-a / k), half()))), simpPow(num(a), half()));
		}
		if (inv)
			return invRoot;
		// sqrt(q) = (2ax + b) sqrt(q)/(4a) + (4ac - b^2)/(8a) * integral of q^(-1/2)
		Expr lin = add(mul(num(Rational(2) * a), X), num(b));
		return add(dv(mul(lin, simpPow(q, half())), num(Rational(4) * a)),
		           mul(num((Rational(4) * a * c - b * b) / (Rational(8) * a)), invRoot));
	}

	// ------------------------------------------------------------ by parts

	Opt byParts(const Expr& f, int depth)
	{
		if (f->kind != Kind::Mul)
			return std::nullopt;
		// split into a polynomial part P and the rest g
		Exprs poly, rest;
		for (const Expr& a : f->args) {
			bool isPoly = true;
			try {
				polyCoefficients(a, x);
			} catch (const Error&) {
				isPoly = false;
			}
			(isPoly ? poly : rest).push_back(a);
		}
		Expr g = simpMul(rest);
		if (!poly.empty() && !rest.empty() && rest.size() == 1) {
			Expr P = expand(simpMul(poly));
			Expr a, b;
			const Expr& h = g;
			bool repeatable = (isCall(h, "exp") || isCall(h, "sin") || isCall(h, "cos") ||
			                   isCall(h, "sinh") || isCall(h, "cosh")) &&
			                  linearIn(h->args[0], x, a, b);
			if (!repeatable && h->kind == Kind::Pow && !containsSym(h->args[0], x) &&
			    linearIn(h->args[1], x, a, b))
				repeatable = true; // c^(ax+b)
			if (repeatable) {
				// P*g = P*G1 - P'*G2 + P''*G3 - ...
				Exprs out;
				Expr G = g, D = P;
				for (int sign = 1; !isZero(D); sign = -sign) {
					Opt Gi = integ(G, depth + 1);
					if (!Gi)
						return std::nullopt;
					G = *Gi;
					out.push_back(mul(mkInt(sign), mul(D, G)));
					D = diff(D, x);
				}
				return simpAdd(out);
			}
			if (h->kind == Kind::Call && h->args.size() == 1 &&
			    (h->name == "ln" || h->name == "asin" || h->name == "acos" ||
			     h->name == "atan" || h->name == "asinh" || h->name == "acosh" ||
			     h->name == "atanh")) {
				// P*L = Q*L - integral of Q*L', with Q = integral of P
				Opt Q = rational(P, depth);
				if (!Q)
					return std::nullopt;
				Opt r = integ(cancel(mul(*Q, diff(h, x))), depth + 1);
				if (!r)
					return std::nullopt;
				return sub(mul(*Q, h), *r);
			}
		}
		// exp(a x + b) * sin(c x + d) and cos
		if (f->args.size() == 2) {
			Expr e1 = f->args[0], e2 = f->args[1];
			if (!isCall(e1, "exp"))
				std::swap(e1, e2);
			Expr a, b, c, d;
			if (isCall(e1, "exp") && (isCall(e2, "sin") || isCall(e2, "cos")) &&
			    linearIn(e1->args[0], x, a, b) && linearIn(e2->args[0], x, c, d)) {
				Expr s = fn("sin", e2->args[0]), co = fn("cos", e2->args[0]);
				Expr den = add(mul(a, a), mul(c, c));
				Expr top = isCall(e2, "sin") ? sub(mul(a, s), mul(c, co)) : add(mul(a, co), mul(c, s));
				return dv(mul(e1, top), den);
			}
		}
		return std::nullopt;
	}

	// -------------------------------------------------------- substitution

	void candidates(const Expr& e, Exprs& out)
	{
		if (!containsSym(e, x) || e->kind == Kind::Sym)
			return;
		if (e->kind == Kind::Call || e->kind == Kind::Pow || e->kind == Kind::Add) {
			bool seen = false;
			for (const Expr& c : out)
				seen = seen || equal(c, e);
			if (!seen)
				out.push_back(e);
		}
		for (const Expr& a : e->args)
			candidates(a, out);
	}

	Opt substitution(const Expr& f, int depth)
	{
		Exprs cs;
		for (const Expr& a : f->kind == Kind::Mul ? f->args : Exprs{f})
			candidates(a, cs);
		Expr t = mkSym("~u" + std::to_string(depth));
		for (const Expr& u : cs) {
			if (equal(u, f))
				continue;
			Expr d = diff(u, x);
			if (isZero(d))
				continue;
			Expr r = mul(f, simpPow(d, mkInt(-1)));
			Expr rt = replaceAll(r, u, t);
			Expr la, lb;
			if (linearIn(u, x, la, lb)) // x = (t - b)/a
				rt = expand(substitute(rt, x, dv(sub(t, lb), la)));
			if (containsSym(rt, x)) {
				rt = replaceAll(cancel(r), u, t);
				if (containsSym(rt, x))
					continue;
			}
			Integrator in(t->name);
			Opt G = in.integ(rt, depth + 1);
			if (G)
				return substitute(*G, t->name, u);
		}
		return std::nullopt;
	}

	// ------------------------------------------------- sin and cos products

	Opt trig(const Expr& f, int depth)
	{
		Expr g = trigreduce(f);
		if (equal(g, f))
			return std::nullopt;
		return integ(g, depth + 1);
	}
};

// (b^r)^n = b^(r*n) for integer n (true for principal roots too), and the
// terms that are fractions of polynomials in x written as one fraction
Expr tidy(const Expr& e, const std::string& x)
{
	std::function<Expr(const Expr&)> flat = [&](const Expr& n) -> Expr {
		switch (n->kind) {
		case Kind::Add:
		case Kind::Mul: {
			Exprs a;
			for (const Expr& c : n->args)
				a.push_back(flat(c));
			return n->kind == Kind::Add ? simpAdd(a) : simpMul(a);
		}
		case Kind::Pow: {
			Expr b = flat(n->args[0]), k = flat(n->args[1]);
			long m;
			if (b->kind == Kind::Pow && isInt(k, m))
				return simpPow(b->args[0], mul(b->args[1], k));
			return simpPow(b, k);
		}
		case Kind::Call: {
			Exprs a;
			for (const Expr& c : n->args)
				a.push_back(flat(c));
			return isElementary(n->name) ? makeFunction(n->name, a) : mkCall(n->name, a);
		}
		default:
			return n;
		}
	};
	Expr r = flat(e);
	Exprs terms = r->kind == Kind::Add ? r->args : Exprs{r};
	Exprs out;
	for (const Expr& t : terms)
		out.push_back(isRationalIn(t, x) && hasVariableDenominator(t) ? cancel(t) : t);
	return simpAdd(out);
}

} // namespace

Expr integrate(const Expr& f, const std::string& x)
{
	Integrator in(x);
	if (f->kind == Kind::Add) { // the parts found, and the rest unevaluated
		Exprs done, left;
		for (const Expr& t : f->args) {
			Opt r = in.integ(t, 0);
			if (r)
				done.push_back(*r);
			else
				left.push_back(t);
		}
		if (!left.empty() && left.size() == f->args.size()) {
			Opt r = in.integ(f, 0); // maybe as a whole (expanded, ...)
			if (r)
				return *r;
		}
		if (!left.empty())
			done.push_back(mkCall("integrate", {simpAdd(left), mkSym(x)}));
		return tidy(simpAdd(done), x);
	}
	Opt r = in.integ(f, 0);
	return r ? tidy(*r, x) : mkCall("integrate", {f, mkSym(x)});
}

} // namespace xyz
