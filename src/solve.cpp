// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// solve(): polynomial equations (after clearing denominators). The
// polynomial is factored first; then each factor is solved by formula:
//  - degree 1 and 2, also with symbols as coefficients
//  - degree 3 (Cardano; with three real roots the trigonometric form) and
//    degree 4 (Ferrari), for numeric coefficients
// Other factors are returned unsolved. Roots of the denominator are left out.
#include "expr.hpp"

#include <algorithm>
#include <map>

namespace xyz {

namespace {

Expr num(const Rational& r) { return mkNum(r); }
Expr frac(long p, long q) { return mkNum(Rational(p) / Rational(q)); }
Expr neg(const Expr& e) { return mul(mkInt(-1), e); }
Expr dv(const Expr& a, const Expr& b) { return mul(a, simpPow(b, mkInt(-1))); }
Expr sqrtOf(const Expr& e) { return simpPow(e, frac(1, 2)); }
Expr omega() { return add(frac(-1, 2), mul(mul(frac(1, 2), sqrtOf(mkInt(3))), mkIm())); }

// the real cube root of s, whose sign is known
Expr cbrtReal(const Expr& s, int sign)
{
	return sign >= 0 ? simpPow(s, frac(1, 3)) : neg(simpPow(expand(neg(s)), frac(1, 3)));
}

// roots of a*x^2 + b*x + c (any coefficients), each once
Exprs quadratic(const Expr& a, const Expr& b, const Expr& c)
{
	Expr d = expand(sub(mul(b, b), mul(mkInt(4), mul(a, c))));
	Expr a2 = mul(mkInt(2), a);
	if (isZero(d))
		return {dv(neg(b), a2)};
	Expr s = sqrtOf(d);
	return {dv(add(neg(b), s), a2), dv(sub(neg(b), s), a2)};
}

// t^3 + p*t + q = 0; 'largest' first when all are real
Exprs depressedCubic(const Rational& p, const Rational& q)
{
	if (p.isZero()) {
		if (q.isZero())
			return {mkInt(0)};
		Expr r = cbrtReal(num(-q), (-q).sign());
		return {r, mul(r, omega()), mul(r, conjugate(omega()))};
	}
	Rational d = q * q / Rational(4) + p * p * p / Rational(27);
	if (d.isZero()) // a double root
		return {num(Rational(3) * q / p), num(Rational(-3) * q / (Rational(2) * p))};
	if (d.sign() > 0) { // one real root
		Rational a = -q / Rational(2);
		Expr sd = sqrtOf(num(d));
		Expr u = cbrtReal(add(num(a), sd), a.sign() >= 0 ? 1 : p.sign());
		Expr v = cbrtReal(sub(num(a), sd), a.sign() <= 0 ? -1 : -p.sign());
		Expr re = neg(mul(frac(1, 2), add(u, v)));
		Expr im = mul(mul(mul(frac(1, 2), sqrtOf(mkInt(3))), sub(u, v)), mkIm());
		return {add(u, v), add(re, im), sub(re, im)};
	}
	// three real roots: t_k = 2*sqrt(-p/3)*cos(acos(3q/(2p)*sqrt(-3/p))/3 - 2*pi*k/3)
	Expr r = mul(mkInt(2), sqrtOf(num(-p / Rational(3))));
	Expr arg = mul(num(Rational(3) * q / (Rational(2) * p)), sqrtOf(num(Rational(-3) / p)));
	Expr theta = mul(frac(1, 3), makeFunction("acos", {arg}));
	Exprs roots;
	for (long k = 0; k < 3; k++)
		roots.push_back(mul(r, makeFunction("cos", {sub(theta, mul(frac(2 * k, 3), mkSym("pi")))})));
	return roots;
}

// a positive real root of x^3 + a*x^2 + b*x + c (which has one): a
// rational one if there is one, else the largest real root by formula
Expr positiveCubicRoot(const Rational& a, const Rational& b, const Rational& c)
{
	auto rr = rationalRoots({c, b, a, Rational(1)});
	if (!rr.empty() && rr.back().sign() > 0)
		return num(rr.back());
	Rational p = b - a * a / Rational(3);
	Rational q = Rational(2) * a * a * a / Rational(27) - a * b / Rational(3) + c;
	Exprs t = depressedCubic(p, q);
	Expr best = t[0];
	if (t.size() == 2 && t[1]->num.compare(t[0]->num) > 0) // a double root
		best = t[1];
	return add(best, num(-a / Rational(3)));
}

Exprs cubic(const std::vector<Rational>& c) // c[0] + ... + c[3] x^3
{
	Rational a = c[2] / c[3], b = c[1] / c[3], d = c[0] / c[3];
	Rational p = b - a * a / Rational(3);
	Rational q = Rational(2) * a * a * a / Rational(27) - a * b / Rational(3) + d;
	Exprs r;
	for (const Expr& t : depressedCubic(p, q))
		r.push_back(add(t, num(-a / Rational(3))));
	return r;
}

Exprs quartic(const std::vector<Rational>& co) // co[0] + ... + co[4] x^4
{
	Rational a = co[3] / co[4], b = co[2] / co[4], c = co[1] / co[4], d = co[0] / co[4];
	Rational a2 = a * a;
	Rational p = b - Rational(3) * a2 / Rational(8);
	Rational q = c - a * b / Rational(2) + a2 * a / Rational(8);
	Rational r = d - a * c / Rational(4) + a2 * b / Rational(16) - Rational(3) * a2 * a2 / Rational(256);
	Expr shift = num(-a / Rational(4));
	Exprs ys;
	if (q.isZero()) { // y^4 + p*y^2 + r: a quadratic in y^2
		for (const Expr& z : quadratic(mkInt(1), num(p), num(r))) {
			ys.push_back(sqrtOf(z));
			ys.push_back(neg(sqrtOf(z)));
		}
	} else {
		// (y^2 + p/2 + m)^2 = (s*y - q/(2s))^2 with s = sqrt(2m), where m > 0
		// is a root of m^3 + p*m^2 + (p^2/4 - r)*m - q^2/8
		Expr m = positiveCubicRoot(p, p * p / Rational(4) - r, -q * q / Rational(8));
		Expr s = sqrtOf(expand(mul(mkInt(2), m)));
		// y^2 - s*y + (base + t) = 0 and y^2 + s*y + (base - t) = 0, whose
		// discriminants are 2m - 4(base +- t) (s^2 = 2m is not simplified)
		Expr base = add(num(p / Rational(2)), m), t = dv(num(q), mul(mkInt(2), s));
		Expr m2 = mul(mkInt(2), m);
		for (int sign : {1, -1}) {
			Expr b = sign > 0 ? neg(s) : s;
			Expr d = expand(sub(m2, mul(mkInt(4), sign > 0 ? add(base, t) : sub(base, t))));
			ys.push_back(mul(frac(1, 2), add(neg(b), sqrtOf(d))));
			ys.push_back(mul(frac(1, 2), sub(neg(b), sqrtOf(d))));
		}
	}
	Exprs roots;
	for (const Expr& y : ys)
		roots.push_back(add(y, shift));
	return roots;
}

} // namespace

Solutions solve(const Expr& e, const std::string& x)
{
	Solutions s;
	auto [p, q] = cancelParts(e);
	if (isZero(p)) {
		s.all = true;
		return s;
	}
	try {
		polyCoefficients(p, x);
	} catch (const Error&) {
		throw Error("solve: can only solve polynomial equations in " + x +
		            " (also with fractions of them)");
	}
	Exprs rational, others;
	for (const auto& [f, mult] : factorPolynomial(p).parts) {
		if (!containsSym(f, x))
			continue; // a factor without x: assumed not to be 0
		auto cm = polyCoefficients(f, x);
		long n = cm.rbegin()->first;
		auto co = [&](long k) { auto it = cm.find(k); return it == cm.end() ? mkInt(0) : it->second; };
		bool numeric = true;
		std::vector<Rational> c;
		for (long k = 0; k <= n; k++) {
			numeric = numeric && co(k)->kind == Kind::Num;
			c.push_back(co(k)->kind == Kind::Num ? co(k)->num : Rational(0));
		}
		Exprs roots;
		if (n == 1)
			roots = {dv(neg(co(0)), co(1))};
		else if (n == 2)
			roots = quadratic(co(2), co(1), co(0));
		else if (n == 3 && numeric)
			roots = cubic(c);
		else if (n == 4 && numeric)
			roots = quartic(c);
		else
			s.unsolved.push_back(f);
		for (const Expr& r : roots)
			(r->kind == Kind::Num ? rational : others).push_back(r);
	}
	std::sort(rational.begin(), rational.end(), [](const Expr& a, const Expr& b) { return a->num.compare(b->num) < 0; });
	for (const Exprs* v : {&rational, &others})
		for (const Expr& r : *v) {
			if (containsSym(q, x) && isIdenticallyZero(substitute(q, x, r)))
				continue; // the denominator is 0 there
			bool seen = false;
			for (const Expr& t : s.roots)
				seen = seen || equal(t, r);
			if (!seen)
				s.roots.push_back(r);
		}
	return s;
}

LinearSolution solveLinear(const Exprs& es, const std::vector<std::string>& xs)
{
	size_t n = xs.size();
	auto notLinear = [](const Expr& e) {
		return Error("solve: only systems of linear equations can be solved, and " +
		             toString(e) + " = 0 is not linear");
	};
	// rows: a[0..n-1] | b, for a . x = b
	std::vector<Exprs> rows;
	for (const Expr& e : es) {
		auto [p, q] = cancelParts(e);
		for (const std::string& x : xs)
			if (containsSym(q, x))
				throw notLinear(e);
		Exprs row;
		Expr rest = p;
		for (const std::string& x : xs) {
			std::map<long, Expr> cm;
			try {
				cm = polyCoefficients(p, x);
			} catch (const Error&) {
				throw notLinear(e);
			}
			if (!cm.empty() && cm.rbegin()->first > 1)
				throw notLinear(e);
			Expr a = cm.count(1) ? cm[1] : mkInt(0);
			for (const std::string& y : xs)
				if (containsSym(a, y))
					throw notLinear(e);
			row.push_back(a);
			rest = substitute(rest, x, mkInt(0));
		}
		row.push_back(neg(rest));
		rows.push_back(row);
	}
	// elimination to reduced row echelon form
	std::vector<long> pivotCol;
	size_t r = 0;
	for (size_t col = 0; col < n && r < rows.size(); col++) {
		size_t piv = r;
		while (piv < rows.size() && isIdenticallyZero(rows[piv][col]))
			piv++;
		if (piv == rows.size())
			continue;
		std::swap(rows[r], rows[piv]);
		Expr inv = simpPow(rows[r][col], mkInt(-1));
		for (Expr& v : rows[r])
			v = cancel(mul(v, inv));
		for (size_t i = 0; i < rows.size(); i++) {
			if (i == r || isIdenticallyZero(rows[i][col]))
				continue;
			Expr f = rows[i][col];
			for (size_t j = 0; j <= n; j++)
				rows[i][j] = cancel(sub(rows[i][j], mul(f, rows[r][j])));
		}
		pivotCol.push_back(static_cast<long>(col));
		r++;
	}
	LinearSolution s;
	for (size_t i = r; i < rows.size(); i++)
		if (!isIdenticallyZero(rows[i][n])) {
			s.none = true; // 0 = b with b != 0
			return s;
		}
	s.free.assign(n, true);
	s.values.resize(n);
	for (size_t j = 0; j < n; j++)
		s.values[j] = mkSym(xs[j]);
	for (size_t i = 0; i < r; i++) {
		size_t col = static_cast<size_t>(pivotCol[i]);
		Expr v = rows[i][n];
		for (size_t j = 0; j < n; j++)
			if (j != col && !isIdenticallyZero(rows[i][j]))
				v = sub(v, mul(rows[i][j], mkSym(xs[j])));
		s.values[col] = cancel(v);
		s.free[col] = false;
	}
	return s;
}

namespace {

using Known = std::vector<std::pair<std::string, Expr>>;

bool hasAny(const Expr& e, const std::vector<std::string>& xs)
{
	for (const std::string& x : xs)
		if (containsSym(e, x))
			return true;
	return false;
}

// conds: expressions divided by on the way, which must not be 0
void systemRec(Exprs fs, std::vector<std::string> xs, Known known, Exprs conds,
               std::vector<Known>& out)
{
	if (out.size() > 1000)
		throw Error("solve: more than 1000 solutions");
	Exprs rest;
	for (const Expr& f : fs) {
		Expr p = cancelParts(f).first;
		if (isIdenticallyZero(p))
			continue;
		if (!hasAny(p, xs))
			return; // 0 = something nonzero: no solution on this branch
		rest.push_back(p);
	}
	fs = rest;
	if (fs.empty()) {
		for (const std::string& x : xs)
			known.emplace_back(x, mkSym(x)); // free
		for (const Expr& c : conds) {
			Expr v = c;
			for (const auto& [name, val] : known)
				v = substitute(v, name, val);
			if (isIdenticallyZero(v))
				return; // divided by 0 on the way
		}
		out.push_back(known);
		return;
	}
	auto put = [](const Exprs& v, const std::string& x, const Expr& val) {
		Exprs r;
		for (const Expr& f : v)
			r.push_back(substitute(f, x, val));
		return r;
	};
	auto without = [&xs](const std::string& x) {
		std::vector<std::string> r;
		for (const std::string& y : xs)
			if (y != x)
				r.push_back(y);
		return r;
	};
	auto record = [](Known k, const std::string& x, const Expr& val) {
		for (auto& [name, v] : k)
			v = cancel(substitute(v, x, val));
		k.emplace_back(x, val);
		return k;
	};
	auto others = [&fs](size_t i) {
		Exprs r;
		for (size_t j = 0; j < fs.size(); j++)
			if (j != i)
				r.push_back(fs[j]);
		return r;
	};
	// an equation linear in some symbol: first with a coefficient without
	// the symbols; later also with one that has them (it must not be 0)
	auto linear = [&](bool anyCoefficient) {
		for (size_t i = 0; i < fs.size(); i++)
			for (const std::string& x : xs) {
				if (!containsSym(fs[i], x))
					continue;
				std::map<long, Expr> cm;
				try {
					cm = polyCoefficients(fs[i], x);
				} catch (const Error&) {
					continue;
				}
				if (cm.rbegin()->first != 1 || isIdenticallyZero(cm[1]) ||
				    (!anyCoefficient && hasAny(cm[1], xs)))
					continue;
				Expr val = cancel(dv(neg(cm.count(0) ? cm[0] : mkInt(0)), cm[1]));
				Exprs cs = conds;
				if (hasAny(cm[1], xs))
					cs.push_back(cm[1]);
				Exprs cs2;
				for (const Expr& c : cs)
					cs2.push_back(substitute(c, x, val));
				systemRec(put(others(i), x, val), without(x), record(known, x, val), cs2, out);
				return true;
			}
		return false;
	};
	if (linear(false))
		return;
	// an equation with only one of the symbols: each of its roots
	for (size_t i = 0; i < fs.size(); i++) {
		std::string only;
		int count = 0;
		for (const std::string& x : xs)
			if (containsSym(fs[i], x)) {
				only = x;
				count++;
			}
		if (count != 1)
			continue;
		Solutions s = solve(fs[i], only);
		if (!s.unsolved.empty())
			throw Error("solve: cannot solve " + toString(s.unsolved[0]) + " = 0 in this system");
		for (const Expr& r : s.roots) {
			Exprs cs;
			for (const Expr& c : conds)
				cs.push_back(substitute(c, only, r));
			systemRec(put(others(i), only, r), without(only), record(known, only, r), cs, out);
		}
		return;
	}
	if (linear(true))
		return;
	throw Error("solve: cannot solve this system (no equation is linear in one of the "
	            "symbols, or has only one of them)");
}

} // namespace

std::vector<Exprs> solveSystem(const Exprs& es, const std::vector<std::string>& xs)
{
	std::vector<Known> sets;
	systemRec(es, xs, {}, {}, sets);
	std::vector<Exprs> out;
	for (const Known& k : sets) {
		Exprs v;
		for (const std::string& x : xs)
			for (const auto& [name, val] : k)
				if (name == x)
					v.push_back(val);
		bool seen = false;
		for (const Exprs& o : out) {
			bool same = true;
			for (size_t i = 0; i < v.size(); i++)
				same = same && equal(o[i], v[i]);
			seen = seen || same;
		}
		if (!seen)
			out.push_back(v);
	}
	return out;
}

} // namespace xyz
