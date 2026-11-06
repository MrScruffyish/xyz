// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// factor(): integers into primes, polynomials into factors over the
// rationals. Kept simple, with bounds:
//  - integers: trial division up to 10^5, then Miller-Rabin and Pollard's
//    rho (Brent). Every number below 2^63 is factored completely; larger
//    ones when the rho method finds the factors within a fixed number of
//    steps (in practice: all prime factors but the largest below about
//    10^10). Numbers with more than 300 digits are refused.
//  - polynomials in one variable: square-free decomposition, rational
//    roots, then Kronecker's method for factors of degree 2 and more
//    (given up when there are too many candidates).
//  - several variables: numbers and common factors are taken out for each
//    variable, then the square-free parts, and homogeneous polynomials in
//    two variables are factored like polynomials in one (x^2 - y^2).
#include "expr.hpp"

#include <algorithm>
#include <functional>
#include <set>

namespace xyz {

namespace {

// ---------------------------------------------------------------- integers

using u64 = unsigned long long;
__extension__ typedef unsigned __int128 u128;

const long kTrialLimit = 100000;
const long kRhoSteps = 400000; // Pollard rho steps for numbers >= 2^63
const size_t kMaxDigits = 300;

u64 mulmod(u64 a, u64 b, u64 m) { return static_cast<u64>(static_cast<u128>(a) * b % m); }

u64 powmod(u64 a, u64 e, u64 m)
{
	u64 r = 1;
	a %= m;
	for (; e; e >>= 1) {
		if (e & 1)
			r = mulmod(r, a, m);
		a = mulmod(a, a, m);
	}
	return r;
}

const u64 kBases[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};

// Miller-Rabin; with these bases exact for all 64-bit numbers
bool isPrime64(u64 n)
{
	if (n < 2)
		return false;
	for (u64 p : kBases)
		if (n % p == 0)
			return n == p;
	u64 d = n - 1;
	int s = 0;
	for (; !(d & 1); d >>= 1)
		s++;
	for (u64 a : kBases) {
		u64 x = powmod(a, d, n);
		if (x == 1 || x == n - 1)
			continue;
		bool composite = true;
		for (int r = 1; r < s && composite; r++) {
			x = mulmod(x, x, n);
			composite = x != n - 1;
		}
		if (composite)
			return false;
	}
	return true;
}

u64 gcd64(u64 a, u64 b)
{
	while (b) {
		u64 t = a % b;
		a = b;
		b = t;
	}
	return a;
}

// a proper factor of the composite n (Pollard's rho, Brent's variant)
u64 rho64(u64 n)
{
	if (n % 2 == 0)
		return 2;
	for (u64 c = 1;; c++) {
		auto f = [&](u64 v) { return (mulmod(v, v, n) + c) % n; };
		u64 y = 2, x = 2, ys = 2, q = 1, g = 1;
		for (u64 r = 1; g == 1; r *= 2) {
			x = y;
			for (u64 i = 0; i < r; i++)
				y = f(y);
			for (u64 k = 0; k < r && g == 1; k += 128) {
				ys = y;
				for (u64 i = 0; i < std::min<u64>(128, r - k); i++) {
					y = f(y);
					q = mulmod(q, x > y ? x - y : y - x, n);
				}
				g = gcd64(q, n);
			}
		}
		if (g == n) // went too far: step by step from ys
			do {
				ys = f(ys);
				g = gcd64(x > ys ? x - ys : ys - x, n);
			} while (g == 1);
		if (g != n)
			return g;
	}
}

void factor64(u64 n, std::vector<BigInt>& out)
{
	if (n == 1)
		return;
	if (isPrime64(n)) {
		out.push_back(BigInt(static_cast<long>(n)));
		return;
	}
	u64 d = rho64(n);
	factor64(d, out);
	factor64(n / d, out);
}

bool fitsLong(const BigInt& n, u64& v)
{
	long l;
	if (!n.toLong(l) || l < 0)
		return false;
	v = static_cast<u64>(l);
	return true;
}

BigInt mulmodBig(const BigInt& a, const BigInt& b, const BigInt& n) { return (a * b).mod(n); }

BigInt powmodBig(BigInt a, BigInt e, const BigInt& n)
{
	BigInt r(1L), two(2L);
	a = a.mod(n);
	while (!e.isZero()) {
		if (e.mod(two).isOne())
			r = mulmodBig(r, a, n);
		a = mulmodBig(a, a, n);
		e = e.divTrunc(two);
	}
	return r;
}

// Miller-Rabin with 12 bases: exact below 3.3*10^24, a probable prime above
bool isPrimeBig(const BigInt& n)
{
	BigInt one(1L), two(2L), m = n - one, d = m;
	long s = 0;
	while (d.mod(two).isZero()) {
		d = d.divTrunc(two);
		s++;
	}
	for (u64 a : kBases) {
		BigInt x = powmodBig(BigInt(static_cast<long>(a)), d, n);
		if (x.isOne() || x == m)
			continue;
		bool composite = true;
		for (long r = 1; r < s && composite; r++) {
			x = mulmodBig(x, x, n);
			composite = x != m;
		}
		if (composite)
			return false;
	}
	return true;
}

// a proper factor of the odd composite n, or 0 when the steps run out
BigInt rhoBig(const BigInt& n, long& steps)
{
	for (long c = 1; steps > 0; c++) {
		BigInt cc(c);
		auto f = [&](const BigInt& v) { return (v * v + cc).mod(n); };
		BigInt y(2L), x(2L), ys(2L), q(1L), g(1L);
		for (long r = 1; g.isOne() && steps > 0; r *= 2) {
			x = y;
			for (long i = 0; i < r; i++)
				y = f(y);
			steps -= r;
			for (long k = 0; k < r && g.isOne(); k += 64) {
				ys = y;
				for (long i = 0; i < std::min(64L, r - k); i++) {
					y = f(y);
					q = mulmodBig(q, (x - y).abs(), n);
				}
				steps -= std::min(64L, r - k);
				g = BigInt::gcd(q, n);
			}
		}
		if (g == n)
			do {
				ys = f(ys);
				g = BigInt::gcd((x - ys).abs(), n);
			} while (g.isOne());
		if (!g.isOne() && g != n)
			return g;
	}
	return BigInt(0L);
}

const std::vector<long>& smallPrimes()
{
	static std::vector<long> primes;
	if (primes.empty()) {
		std::vector<bool> composite(kTrialLimit + 1);
		for (long i = 2; i <= kTrialLimit; i++) {
			if (composite[static_cast<size_t>(i)])
				continue;
			primes.push_back(i);
			for (long j = i * i; j <= kTrialLimit; j += i)
				composite[static_cast<size_t>(j)] = true;
		}
	}
	return primes;
}

void factorBig(const BigInt& n, std::vector<BigInt>& out, long& steps)
{
	u64 v;
	if (n.isOne())
		return;
	if (fitsLong(n, v)) {
		factor64(v, out);
		return;
	}
	if (isPrimeBig(n)) {
		out.push_back(n);
		return;
	}
	BigInt d = rhoBig(n, steps);
	if (d.isZero())
		throw Error("factor: cannot factor " + n.toString() +
		            " within the bound: it has no prime factor below 10^5, and Pollard's "
		            "rho method (good for prime factors up to about 10^11) found none");
	factorBig(d, out, steps);
	factorBig(n.divTrunc(d), out, steps);
}

// n > 0 as primes with multiplicities, ascending
std::vector<std::pair<BigInt, long>> primeFactors(BigInt n)
{
	if (n.toString().size() > kMaxDigits)
		throw Error("factor: numbers with more than " + std::to_string(kMaxDigits) +
		            " digits are not factored");
	std::vector<BigInt> ps;
	u64 v;
	if (fitsLong(n, v)) {
		for (; v % 2 == 0; v /= 2)
			ps.push_back(BigInt(2L));
		factor64(v, ps);
	} else {
		for (long p : smallPrimes()) {
			BigInt bp(p);
			if ((bp * bp).compare(n) > 0)
				break;
			while (n.mod(bp).isZero()) {
				ps.push_back(bp);
				n = n.divTrunc(bp);
			}
			if (fitsLong(n, v))
				break;
		}
		long steps = kRhoSteps;
		factorBig(n, ps, steps);
	}
	std::sort(ps.begin(), ps.end(), [](const BigInt& a, const BigInt& b) { return a.compare(b) < 0; });
	std::vector<std::pair<BigInt, long>> r;
	for (const BigInt& p : ps) {
		if (!r.empty() && r.back().first == p)
			r.back().second++;
		else
			r.emplace_back(p, 1);
	}
	return r;
}

// all positive divisors of n > 0, or false if there are more than limit
bool divisors(const BigInt& n, std::vector<BigInt>& out, size_t limit)
{
	out = {BigInt(1L)};
	for (const auto& [p, k] : primeFactors(n)) {
		size_t m = out.size();
		BigInt pk(1L);
		for (long i = 1; i <= k; i++) {
			pk = pk * p;
			for (size_t j = 0; j < m; j++)
				out.push_back(out[j] * pk);
			if (out.size() > limit)
				return false;
		}
	}
	return true;
}

Expr factorNumber(const Rational& r)
{
	if (r.isZero() || r.abs().isOne())
		return mkNum(r);
	auto num = primeFactors(r.numerator().abs());
	auto den = primeFactors(r.denominator());
	if (den.empty() && num.size() == 1 && num[0].second == 1)
		return mkNum(r); // a prime
	Exprs f;
	if (r.sign() < 0)
		f.push_back(mkInt(-1));
	for (const auto& [p, k] : num)
		f.push_back(k == 1 ? mkNum(Rational(p, BigInt(1L))) : mkPow(mkNum(Rational(p, BigInt(1L))), mkInt(k)));
	for (const auto& [p, k] : den)
		f.push_back(mkPow(mkNum(Rational(p, BigInt(1L))), mkInt(-k)));
	return mkHeld(mkCall("%fac", f), mkNum(r));
}

// ----------------------------------------- polynomials in one variable
// dense, with rational coefficients: c[i] is the coefficient of x^i

using QPoly = std::vector<Rational>;

void trim(QPoly& p)
{
	while (!p.empty() && p.back().isZero())
		p.pop_back();
}

long degree(const QPoly& p) { return static_cast<long>(p.size()) - 1; } // -1 for 0

QPoly sub(const QPoly& a, const QPoly& b)
{
	QPoly r(std::max(a.size(), b.size()));
	for (size_t i = 0; i < r.size(); i++)
		r[i] = (i < a.size() ? a[i] : Rational(0)) - (i < b.size() ? b[i] : Rational(0));
	trim(r);
	return r;
}

QPoly deriv(const QPoly& p)
{
	QPoly r;
	for (size_t i = 1; i < p.size(); i++)
		r.push_back(p[i] * Rational(static_cast<long>(i)));
	trim(r);
	return r;
}

// a = q*b + r
void divmod(const QPoly& a, const QPoly& b, QPoly& q, QPoly& r)
{
	r = a;
	q.assign(a.size() >= b.size() ? a.size() - b.size() + 1 : 0, Rational(0));
	while (degree(r) >= degree(b)) {
		size_t k = r.size() - b.size();
		Rational c = r.back() / b.back();
		q[k] = c;
		for (size_t i = 0; i < b.size(); i++)
			r[i + k] = r[i + k] - c * b[i];
		r.pop_back();
		trim(r);
	}
	trim(q);
}

QPoly quo(const QPoly& a, const QPoly& b)
{
	QPoly q, r;
	divmod(a, b, q, r);
	return q;
}

QPoly gcdQ(QPoly a, QPoly b)
{
	while (!b.empty()) {
		QPoly q, r;
		divmod(a, b, q, r);
		a = std::move(b);
		b = std::move(r);
	}
	if (!a.empty()) {
		Rational l = a.back();
		for (Rational& c : a)
			c = c / l;
	}
	return a;
}

// integer coefficients without a common factor, positive leading coefficient
QPoly primitive(QPoly p)
{
	Rational g(0);
	for (const Rational& c : p)
		g = gcd(g, c);
	if (p.back().sign() < 0)
		g = -g;
	for (Rational& c : p)
		c = c / g;
	return p;
}

Rational evalAt(const QPoly& p, const Rational& x)
{
	Rational v(0);
	for (size_t i = p.size(); i-- > 0;)
		v = v * x + p[i];
	return v;
}

bool isIntPoly(const QPoly& p)
{
	for (const Rational& c : p)
		if (!c.isInt())
			return false;
	return true;
}

// the rational roots of the primitive integer polynomial p, ascending
std::vector<Rational> ratRoots(QPoly p)
{
	std::vector<Rational> roots;
	if (p.size() >= 2 && p[0].isZero()) {
		roots.push_back(Rational(0));
		while (p.size() >= 2 && p[0].isZero())
			p.erase(p.begin());
	}
	if (degree(p) < 1)
		return roots;
	std::vector<BigInt> ps, qs;
	try {
		if (!divisors(p[0].numerator().abs(), ps, 5000) ||
		    !divisors(p.back().numerator().abs(), qs, 5000))
			return roots; // too many candidates: only the ones found so far
	} catch (const Error&) {
		return roots; // the coefficients are too large to factor
	}
	for (const BigInt& a : ps)
		for (const BigInt& b : qs) {
			if (!BigInt::gcd(a, b).isOne())
				continue;
			for (int s : {1, -1}) {
				Rational r(s > 0 ? a : -a, b);
				if (evalAt(p, r).isZero())
					roots.push_back(r);
			}
		}
	std::sort(roots.begin(), roots.end(), [](const Rational& a, const Rational& b) { return a.compare(b) < 0; });
	return roots;
}

// Kronecker: a factor of degree k of the primitive integer polynomial g, or {}
QPoly kroneckerFactor(const QPoly& g, long k)
{
	const size_t kLimit = 20000; // candidate polynomials tried at most
	struct Point {
		Rational a;
		std::vector<BigInt> divs;
	};
	std::vector<Point> pts;
	for (long i = 0; i <= 40 && pts.size() < static_cast<size_t>(3 * k + 3); i++) {
		Rational a(i % 2 ? (i + 1) / 2 : -(i / 2));
		Rational v = evalAt(g, a);
		if (v.isZero())
			return {}; // a rational root: not expected here
		Point pt{a, {}};
		try {
			if (!divisors(v.numerator().abs(), pt.divs, kLimit))
				continue;
		} catch (const Error&) {
			continue;
		}
		pts.push_back(std::move(pt));
	}
	if (pts.size() < static_cast<size_t>(k + 1))
		return {};
	std::stable_sort(pts.begin(), pts.end(),
	                 [](const Point& a, const Point& b) { return a.divs.size() < b.divs.size(); });
	pts.resize(static_cast<size_t>(k + 1));
	double total = 1;
	for (size_t i = 0; i < pts.size(); i++)
		total *= static_cast<double>(pts[i].divs.size()) * (i ? 2 : 1);
	if (total > kLimit)
		return {};
	// all values: the first one positive, the others with both signs
	std::vector<std::vector<Rational>> vals;
	for (size_t i = 0; i < pts.size(); i++) {
		std::vector<Rational> v;
		for (const BigInt& d : pts[i].divs) {
			v.push_back(Rational(d, BigInt(1L)));
			if (i)
				v.push_back(Rational(-d, BigInt(1L)));
		}
		vals.push_back(std::move(v));
	}
	std::vector<size_t> idx(pts.size(), 0);
	for (;;) {
		// Newton interpolation through (a_i, v_i)
		size_t n = pts.size();
		std::vector<Rational> dd(n);
		for (size_t i = 0; i < n; i++)
			dd[i] = vals[i][idx[i]];
		for (size_t j = 1; j < n; j++)
			for (size_t i = n - 1; i >= j; i--)
				dd[i] = (dd[i] - dd[i - 1]) / (pts[i].a - pts[i - j].a);
		QPoly h{dd[n - 1]};
		for (size_t i = n - 1; i-- > 0;) { // h = h*(x - a_i) + dd[i]
			QPoly t(h.size() + 1, Rational(0));
			for (size_t m = 0; m < h.size(); m++) {
				t[m + 1] = t[m + 1] + h[m];
				t[m] = t[m] - h[m] * pts[i].a;
			}
			t[0] = t[0] + dd[i];
			h = std::move(t);
		}
		trim(h);
		if (degree(h) == k && isIntPoly(h)) {
			QPoly q, r;
			divmod(g, h, q, r);
			if (r.empty() && isIntPoly(q))
				return primitive(h);
		}
		size_t i = 0;
		for (; i < n; i++) {
			if (++idx[i] < vals[i].size())
				break;
			idx[i] = 0;
		}
		if (i == n)
			return {};
	}
}

// square-free factors of p with multiplicities (Yun)
std::vector<std::pair<QPoly, long>> squareFree(const QPoly& f)
{
	std::vector<std::pair<QPoly, long>> out;
	QPoly fp = deriv(f);
	QPoly a = gcdQ(f, fp);
	QPoly b = quo(f, a), c = quo(fp, a), d = sub(c, deriv(b));
	for (long i = 1; degree(b) > 0; i++) {
		a = gcdQ(b, d);
		if (degree(a) > 0)
			out.emplace_back(primitive(a), i);
		b = quo(b, a);
		c = quo(d, a);
		d = sub(c, deriv(b));
	}
	return out;
}

// irreducible factors (as far as found) of a polynomial with degree >= 1
std::vector<std::pair<QPoly, long>> factorUni(const QPoly& f)
{
	std::vector<std::pair<QPoly, long>> out;
	for (auto [s, m] : squareFree(primitive(f))) {
		for (const Rational& r : ratRoots(s)) { // q*x - p
			out.push_back({{Rational(-r.numerator(), BigInt(1L)), Rational(r.denominator(), BigInt(1L))}, m});
			s = quo(s, {-r, Rational(1)});
		}
		if (degree(s) < 1)
			continue;
		s = primitive(s);
		std::function<void(const QPoly&)> split = [&](const QPoly& g) {
			for (long k = 2; 2 * k <= degree(g); k++) {
				QPoly h = kroneckerFactor(g, k);
				if (!h.empty()) {
					out.emplace_back(h, m);
					split(primitive(quo(g, h)));
					return;
				}
			}
			out.emplace_back(g, m);
		};
		if (degree(s) >= 4)
			split(s);
		else
			out.emplace_back(s, m);
	}
	return out;
}

bool toQPoly(const Expr& p, const std::string& x, QPoly& out)
{
	out.clear();
	for (const auto& [k, c] : polyCoefficients(p, x)) {
		if (c->kind != Kind::Num)
			return false;
		if (out.size() <= static_cast<size_t>(k))
			out.resize(static_cast<size_t>(k) + 1, Rational(0));
		out[static_cast<size_t>(k)] = c->num;
	}
	return true;
}

Expr fromQPoly(const QPoly& p, const Expr& x)
{
	Exprs t;
	for (size_t i = 0; i < p.size(); i++)
		if (!p[i].isZero())
			t.push_back(mul(mkNum(p[i]), simpPow(x, mkInt(static_cast<long>(i)))));
	return simpAdd(t);
}

// ------------------------------------------- polynomials in several symbols

// Non-polynomial parts (calls, roots, im) are replaced by symbols ~f1, ...
struct Atoms {
	std::vector<std::pair<Expr, std::string>> list;

	Expr put(const Expr& e)
	{
		long k;
		switch (e->kind) {
		case Kind::Num:
			return e;
		case Kind::Sym:
			return isImUnit(e) ? atom(e) : e;
		case Kind::Add:
		case Kind::Mul: {
			Exprs a;
			for (const Expr& x : e->args)
				a.push_back(put(x));
			return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
		}
		case Kind::Pow:
			if (isInt(e->args[1], k) && k >= 0)
				return simpPow(put(e->args[0]), e->args[1]);
			return atom(e);
		default:
			return atom(e);
		}
	}

	Expr atom(const Expr& e)
	{
		for (const auto& [a, name] : list)
			if (equal(a, e))
				return mkSym(name);
		list.emplace_back(e, "~f" + std::to_string(list.size() + 1));
		return mkSym(list.back().second);
	}

	Expr back(const Expr& e) const
	{
		Expr r = e;
		for (const auto& [a, name] : list)
			r = substitute(r, name, a);
		return r;
	}
};

void symbols(const Expr& e, std::set<std::string>& out)
{
	if (e->kind == Kind::Sym)
		out.insert(e->name);
	for (const Expr& a : e->args)
		symbols(a, out);
}

// leading numeric coefficient in the lexicographic order of vars; it is
// multiplicative: lead(a*b) = lead(a)*lead(b)
Rational leadNum(Expr p, const std::vector<std::string>& vars)
{
	for (const std::string& v : vars)
		if (containsSym(p, v))
			p = polyCoefficients(p, v).rbegin()->second;
	return p->num;
}

// a/b for polynomials when b divides a
Expr exactQuo(const Expr& a, const Expr& b)
{
	auto [n, d] = cancelParts(mul(a, simpPow(b, mkInt(-1))));
	return expand(mul(n, simpPow(d, mkInt(-1))));
}

bool isHomogeneous(const Expr& p, long& total)
{
	Exprs terms = p->kind == Kind::Add ? p->args : Exprs{p};
	total = -1;
	for (const Expr& t : terms) {
		long d = 0;
		Exprs fs = t->kind == Kind::Mul ? t->args : Exprs{t};
		for (const Expr& f : fs) {
			long k;
			if (f->kind == Kind::Sym)
				d += 1;
			else if (f->kind == Kind::Pow && isInt(f->args[1], k))
				d += k;
		}
		if (total >= 0 && d != total)
			return false;
		total = d;
	}
	return true;
}

using Parts = std::vector<std::pair<Expr, long>>;

// the square-free, primitive p (no common factors): split it further
void splitSquareFree(const Expr& p, long m, Parts& out)
{
	std::set<std::string> vs;
	symbols(p, vs);
	if (vs.size() == 1) {
		Expr x = mkSym(*vs.begin());
		QPoly q;
		if (toQPoly(p, x->name, q)) {
			for (const auto& [f, k] : factorUni(q))
				out.emplace_back(fromQPoly(f, x), m * k);
			return;
		}
	}
	long total;
	if (vs.size() == 2 && isHomogeneous(p, total)) {
		// p(x, y) = y^total * p(x/y, 1)
		Expr x = mkSym(*vs.begin()), y = mkSym(*vs.rbegin());
		QPoly q;
		if (toQPoly(substitute(p, y->name, mkInt(1)), x->name, q)) {
			for (const auto& [f, k] : factorUni(q)) {
				Exprs t;
				for (size_t i = 0; i < f.size(); i++)
					t.push_back(mul(mkNum(f[i]), mul(simpPow(x, mkInt(static_cast<long>(i))),
					                                  simpPow(y, mkInt(degree(f) - static_cast<long>(i))))));
				out.emplace_back(simpAdd(t), m * k);
			}
			return;
		}
	}
	out.emplace_back(p, m);
}

void factorRec(Expr p, long m, Parts& out)
{
	if (p->kind == Kind::Num)
		return;
	std::set<std::string> vs;
	symbols(p, vs);
	// common powers of each symbol, then the content for each symbol
	for (const std::string& v : vs) {
		long k = polyCoefficients(p, v).begin()->first;
		if (k > 0) {
			out.emplace_back(mkSym(v), m * k);
			p = exactQuo(p, simpPow(mkSym(v), mkInt(k)));
		}
	}
	for (const std::string& v : vs) {
		if (!containsSym(p, v))
			continue;
		Expr g;
		for (const auto& [k, c] : polyCoefficients(p, v))
			g = g ? polyGcd(g, c) : c;
		if (g->kind != Kind::Num) {
			factorRec(g, m, out);
			factorRec(exactQuo(p, g), m, out);
			return;
		}
	}
	if (p->kind == Kind::Num)
		return;
	vs.clear();
	symbols(p, vs); // some may be gone with the common factors
	if (vs.size() == 1) {
		splitSquareFree(p, m, out); // square-free decomposition there
		return;
	}
	// square-free decomposition in the first symbol (Yun)
	const std::string& x = *vs.begin();
	Expr dp = diff(p, x);
	Expr a = polyGcd(p, dp);
	Expr b = exactQuo(p, a), c = exactQuo(dp, a), d = expand(sub(c, diff(b, x)));
	for (long i = 1; containsSym(b, x); i++) {
		a = polyGcd(b, d);
		if (a->kind != Kind::Num)
			splitSquareFree(a, m * i, out);
		b = exactQuo(b, a);
		c = exactQuo(d, a);
		d = expand(sub(c, diff(b, x)));
	}
}

} // namespace

std::vector<Rational> rationalRoots(const std::vector<Rational>& c)
{
	QPoly p = c;
	trim(p);
	if (degree(p) < 1)
		return {};
	return ratRoots(primitive(p));
}

Factored factorPolynomial(const Expr& p)
{
	Atoms atoms;
	Expr q = expand(atoms.put(p));
	Factored r{Rational(1), {}};
	if (q->kind == Kind::Num) {
		r.content = q->num;
		return r;
	}
	Parts parts;
	factorRec(q, 1, parts);
	std::set<std::string> vs;
	symbols(q, vs);
	std::vector<std::string> vars(vs.begin(), vs.end());
	// each factor primitive with a positive leading coefficient
	Rational lead(1);
	for (auto& [f, m] : parts) {
		Exprs terms = f->kind == Kind::Add ? f->args : Exprs{f};
		Rational g(0);
		for (const Expr& t : terms)
			g = gcd(g, t->kind == Kind::Num ? t->num
			           : t->kind == Kind::Mul && t->args[0]->kind == Kind::Num ? t->args[0]->num
			                                                                  : Rational(1));
		if (leadNum(f, vars).sign() < 0)
			g = -g;
		f = expand(mul(mkNum(Rational(1) / g), f));
		Rational l = leadNum(f, vars);
		for (long i = 0; i < m; i++)
			lead = lead * l;
	}
	r.content = leadNum(q, vars) / lead;
	// equal factors together, in a fixed order
	std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return compare(a.first, b.first) < 0; });
	for (const auto& [f, m] : parts) {
		if (!r.parts.empty() && equal(r.parts.back().first, f))
			r.parts.back().second += m;
		else
			r.parts.emplace_back(f, m);
	}
	for (auto& part : r.parts)
		part.first = atoms.back(part.first);
	return r;
}

Expr factor(const Expr& e)
{
	if (e->kind == Kind::Num)
		return factorNumber(e->num);
	auto [p, q] = cancelParts(e);
	Factored fp = factorPolynomial(p), fq = factorPolynomial(q);
	Rational content = fp.content / fq.content;
	if (fq.parts.empty() && fp.parts.size() == 1 && fp.parts[0].second == 1 && content.isOne())
		return e; // nothing to factor
	Exprs shown;
	if (!content.isOne())
		shown.push_back(mkNum(content));
	for (const auto& [f, m] : fp.parts)
		shown.push_back(m == 1 ? f : mkPow(f, mkInt(m)));
	for (const auto& [f, m] : fq.parts)
		shown.push_back(mkPow(f, mkInt(-m)));
	return mkHeld(shown.size() == 1 ? shown[0] : mkNode(Kind::Mul, shown), e);
}

Expr substitute(const Expr& e, const std::string& x, const Expr& v)
{
	switch (e->kind) {
	case Kind::Sym:
		return e->name == x ? v : e;
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		for (const Expr& t : e->args)
			a.push_back(substitute(t, x, v));
		return e->kind == Kind::Add ? simpAdd(a) : simpMul(a);
	}
	case Kind::Pow:
		return simpPow(substitute(e->args[0], x, v), substitute(e->args[1], x, v));
	case Kind::Call: {
		Exprs a;
		for (const Expr& t : e->args)
			a.push_back(substitute(t, x, v));
		const std::string& f = e->name;
		if (isElementary(f) || f == "log")
			return makeFunction(f, a);
		if (f == "Re") return realPart(a[0]);
		if (f == "Im") return imagPart(a[0]);
		if (f == "conj") return conjugate(a[0]);
		if (f == "abs") return absValue(a[0]);
		return mkCall(f, a);
	}
	default:
		return e;
	}
}

} // namespace xyz
