// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Decimal approximations for num(). Numbers are fixed point: a BigInt v
// stands for v / 10^S. All arithmetic is done by bcl on these integers,
// with guard digits beyond the digits asked for:
//   exp:  halve the argument until it is small, Taylor series, square back
//   ln:   x = 2^k*y with y in [1/2, 1), ln(y) = 2*atanh((y-1)/(y+1))
//   pi:   Machin's formula 16*atan(1/5) - 4*atan(1/239)
//   sin, cos: reduce by multiples of pi/2, Taylor series
//   atan: atan(x) = 2*atan(x/(1+sqrt(1+x^2))) until x is small, series
// Complex numbers work for + - * /, powers, exp, ln, sin and cos.
// The result is rounded (or truncated, with --trunc) to the digits asked
// for. There is no proof that every digit is right: the guard digits make
// a wrong last digit very unlikely, but not impossible.
#include "expr.hpp"

#include <map>
#include <optional>

namespace xyz {

namespace {

bool g_truncate = false;

// pi and ln(2) are needed again and again: computed once per scale
std::map<size_t, BigInt> g_pi, g_ln2;

BigInt pow10(size_t n) { return BigInt(10L).pow(n); }

// a/b rounded to the nearest integer, halves away from zero
BigInt divRound(const BigInt& a, const BigInt& b)
{
	if (b.isZero())
		throw Error("division by zero");
	bool negative = (a.sign() < 0) != (b.sign() < 0);
	BigInt aa = a.abs(), bb = b.abs();
	BigInt q = (aa * BigInt(2L) + bb).divTrunc(bb * BigInt(2L));
	return negative ? -q : q;
}

struct Cx {
	BigInt re, im;
};

class Fixed {
public:
	explicit Fixed(size_t scale) : S(scale), one(pow10(scale)) {}

	size_t S;
	BigInt one;

	BigInt from(const Rational& r) const { return divRound(r.numerator() * one, r.denominator()); }
	BigInt mul(const BigInt& a, const BigInt& b) const { return divRound(a * b, one); }
	BigInt div(const BigInt& a, const BigInt& b) const { return divRound(a * one, b); }
	BigInt sqrt(const BigInt& a) const
	{
		if (a.sign() < 0)
			throw std::logic_error("sqrt of a negative number");
		return (a * one).isqrt();
	}

	// from this scale to another one
	BigInt to(const BigInt& v, size_t s2) const
	{
		if (s2 >= S)
			return v * pow10(s2 - S);
		return divRound(v, pow10(S - s2));
	}

	// number of decimal digits in the integer part of |v|
	long intDigits(const BigInt& v) const
	{
		return static_cast<long>(v.abs().divTrunc(one).toString().size());
	}

	BigInt pi() const
	{
		auto it = g_pi.find(S);
		if (it != g_pi.end())
			return it->second;
		Fixed f(S + 10);
		BigInt p = f.atanInv(5) * BigInt(16L) - f.atanInv(239) * BigInt(4L);
		return g_pi.emplace(S, f.to(p, S)).first->second;
	}

	BigInt ln2() const
	{
		auto it = g_ln2.find(S);
		if (it != g_ln2.end())
			return it->second;
		return g_ln2.emplace(S, atanh2(div(one, BigInt(3L) * one))).first->second;
	}

	BigInt exp(const BigInt& x) const
	{
		// exp(230000) has 100000 digits: beyond that, num gives up rather
		// than computing for minutes
		long m;
		if (!x.abs().divTrunc(one).toLong(m) || (x.sign() > 0 && m > 230000L))
			throw Error("num: number too large (more than 100000 digits)");
		if (x.sign() < 0 && m > 230000L)
			return BigInt(0L); // exp(-230000) is 0 to any number of digits we print
		// the result has about 0.44*m integer digits: keep that many more
		size_t extra = 20 + static_cast<size_t>(x.sign() > 0 ? m / 2 : 0);
		Fixed f(S + extra);
		BigInt y = to(x, f.S);
		// halve until |y| < 10^-r: the series then needs about S/r terms,
		// and squaring back costs one multiplication per halving
		size_t r = 1;
		while (r * r < f.S / 4)
			r++;
		BigInt small = f.one.divTrunc(pow10(r));
		int k = 0;
		while (y.abs().compare(small) > 0) {
			y = divRound(y, BigInt(2L));
			k++;
		}
		Fixed g(f.S + static_cast<size_t>(k) / 3 + 5); // squaring doubles the error
		BigInt z = f.to(y, g.S);
		BigInt sum = g.one, term = g.one;
		for (long n = 1; !term.isZero(); n++) {
			term = divRound(term * z, g.one * BigInt(n));
			sum = sum + term;
		}
		for (int i = 0; i < k; i++)
			sum = g.mul(sum, sum);
		return g.to(sum, S);
	}

	// 2*atanh(z) = 2*(z + z^3/3 + z^5/5 + ...), |z| <= 1/3
	BigInt atanh2(const BigInt& z) const
	{
		BigInt z2 = mul(z, z), term = z, sum = BigInt(0L);
		for (long n = 1; !term.isZero(); n += 2) {
			sum = sum + term.divTrunc(BigInt(n));
			term = mul(term, z2);
		}
		return sum * BigInt(2L);
	}

	BigInt ln(const BigInt& x) const
	{
		if (x.sign() <= 0)
			throw std::logic_error("ln of a non-positive number");
		// x = 2^k*y: only k*ln(2) needs a few more digits (k has about
		// log10(k) digits), not the size of x itself
		long digits = intDigits(x) + 2;
		Fixed f(S + 20 + std::to_string(digits * 4).size());
		BigInt y = to(x, f.S);
		long k = 0;
		BigInt halfOne = f.one.divTrunc(BigInt(2L));
		if (digits > 4) { // jump close to the right power of 2: 10^d ~ 2^(3.32*d)
			k = (digits - 3) * 3321 / 1000;
			y = divRound(y, BigInt(2L).pow(static_cast<unsigned long>(k)));
		}
		while (y.compare(f.one) >= 0) {
			y = divRound(y, BigInt(2L));
			k++;
		}
		while (y.compare(halfOne) < 0) {
			y = y * BigInt(2L);
			k--;
		}
		BigInt r = f.atanh2(f.div(y - f.one, y + f.one));
		if (k != 0)
			r = r + f.ln2() * BigInt(k);
		return f.to(r, S);
	}

	// atan(1/n) for an integer n > 1, by the series
	BigInt atanInv(long n) const
	{
		BigInt nn(n), n2(n * n);
		BigInt term = one.divTrunc(nn), sum = BigInt(0L);
		for (long k = 1, sign = 1; !term.isZero(); k += 2, sign = -sign) {
			BigInt t = term.divTrunc(BigInt(k));
			sum = sign > 0 ? sum + t : sum - t;
			term = term.divTrunc(n2);
		}
		return sum;
	}

	BigInt atan(const BigInt& x) const
	{
		if (x.sign() < 0)
			return -atan(-x);
		if (x.compare(one) > 0) // atan(x) = pi/2 - atan(1/x)
			return divRound(pi(), BigInt(2L)) - atan(div(one, x));
		Fixed f(S + 20);
		BigInt y = to(x, f.S);
		BigInt tenth = f.one.divTrunc(BigInt(10L));
		long r = 0;
		while (y.compare(tenth) > 0) { // halve the angle
			y = f.div(y, f.one + f.sqrt(f.one + f.mul(y, y)));
			r++;
		}
		BigInt y2 = f.mul(y, y), term = y, sum = BigInt(0L);
		for (long n = 1, sign = 1; !term.isZero(); n += 2, sign = -sign) {
			BigInt t = term.divTrunc(BigInt(n));
			sum = sign > 0 ? sum + t : sum - t;
			term = f.mul(term, y2);
		}
		return f.to(sum * BigInt(2L).pow(static_cast<unsigned long>(r)), S);
	}

	// sin and cos of a real number
	void sincos(const BigInt& x, BigInt& s, BigInt& c) const
	{
		Fixed f(S + 20 + static_cast<size_t>(intDigits(x)));
		BigInt y = to(x, f.S);
		BigInt halfPi = divRound(f.pi(), BigInt(2L));
		BigInt n = divRound(y, halfPi);
		BigInt r = y - n * halfPi; // |r| <= pi/4
		BigInt r2 = f.mul(r, r);
		BigInt sinr = BigInt(0L), cosr = BigInt(0L);
		BigInt term = r;
		for (long k = 1; !term.isZero(); k += 2) { // r - r^3/3! + ...
			sinr = sinr + term;
			term = -divRound(f.mul(term, r2), BigInt((k + 1) * (k + 2)));
		}
		term = f.one;
		for (long k = 0; !term.isZero(); k += 2) { // 1 - r^2/2! + ...
			cosr = cosr + term;
			term = -divRound(f.mul(term, r2), BigInt((k + 1) * (k + 2)));
		}
		long q = 0;
		BigInt four(4L);
		BigInt m = n.mod(four); // sign follows n
		m.toLong(q);
		q = ((q % 4) + 4) % 4;
		BigInt sv[] = {sinr, cosr, -sinr, -cosr};
		BigInt cv[] = {cosr, -sinr, -cosr, sinr};
		s = f.to(sv[q], S);
		c = f.to(cv[q], S);
	}

	// ---------------------------------------------------- complex numbers

	Cx cmul(const Cx& a, const Cx& b) const
	{
		return {mul(a.re, b.re) - mul(a.im, b.im), mul(a.re, b.im) + mul(a.im, b.re)};
	}

	Cx cdiv(const Cx& a, const Cx& b) const
	{
		BigInt d = mul(b.re, b.re) + mul(b.im, b.im);
		if (d.isZero())
			throw Error("division by zero");
		Cx n = cmul(a, {b.re, -b.im});
		return {div(n.re, d), div(n.im, d)};
	}

	Cx cexp(const Cx& z) const
	{
		BigInt e = exp(z.re);
		if (z.im.isZero())
			return {e, BigInt(0L)};
		BigInt s, c;
		sincos(z.im, s, c);
		return {mul(e, c), mul(e, s)};
	}

	BigInt atan2(const BigInt& y, const BigInt& x) const
	{
		if (x.sign() > 0)
			return atan(div(y, x));
		BigInt p = pi();
		if (x.sign() < 0)
			return y.sign() >= 0 ? atan(div(y, x)) + p : atan(div(y, x)) - p;
		return y.sign() >= 0 ? divRound(p, BigInt(2L)) : -divRound(p, BigInt(2L));
	}

	Cx cln(const Cx& z) const
	{
		if (z.re.isZero() && z.im.isZero())
			throw Error("ln(0) is undefined");
		if (z.im.isZero() && z.re.sign() > 0)
			return {ln(z.re), BigInt(0L)};
		BigInt r = sqrt(mul(z.re, z.re) + mul(z.im, z.im));
		return {ln(r), atan2(z.im, z.re)};
	}
};

Cx creal(const BigInt& v) { return {v, BigInt(0L)}; }

std::optional<Cx> approx(const Expr& e, const Fixed& f);

std::optional<Cx> approxCall(const Expr& e, const Fixed& f)
{
	const std::string& n = e->name;
	std::vector<Cx> a;
	for (const Expr& x : e->args) {
		auto v = approx(x, f);
		if (!v)
			return std::nullopt;
		a.push_back(*v);
	}
	if (a.size() != 1)
		return std::nullopt;
	const Cx& z = a[0];
	bool real = z.im.isZero();
	if (n == "Re")
		return creal(z.re);
	if (n == "Im")
		return creal(z.im);
	if (n == "conj")
		return Cx{z.re, -z.im};
	if (n == "abs")
		return creal(f.sqrt(f.mul(z.re, z.re) + f.mul(z.im, z.im)));
	if (n == "exp")
		return f.cexp(z);
	if (n == "ln")
		return f.cln(z);
	if (n == "sin" || n == "cos" || n == "tan") {
		Cx s, c;
		if (real) {
			f.sincos(z.re, s.re, c.re);
			s.im = c.im = BigInt(0L);
		} else { // sin(a+bi) = sin(a)cosh(b) + i cos(a)sinh(b)
			BigInt sa, ca;
			f.sincos(z.re, sa, ca);
			BigInt eb = f.exp(z.im), emb = f.exp(-z.im);
			BigInt ch = divRound(eb + emb, BigInt(2L)), sh = divRound(eb - emb, BigInt(2L));
			s = {f.mul(sa, ch), f.mul(ca, sh)};
			c = {f.mul(ca, ch), -f.mul(sa, sh)};
		}
		if (n == "sin")
			return s;
		if (n == "cos")
			return c;
		return f.cdiv(s, c);
	}
	if (n == "sinh" || n == "cosh" || n == "tanh") {
		Cx ep = f.cexp(z), em = f.cexp(Cx{-z.re, -z.im});
		Cx s{divRound(ep.re - em.re, BigInt(2L)), divRound(ep.im - em.im, BigInt(2L))};
		Cx c{divRound(ep.re + em.re, BigInt(2L)), divRound(ep.im + em.im, BigInt(2L))};
		if (n == "sinh")
			return s;
		if (n == "cosh")
			return c;
		return f.cdiv(s, c);
	}
	if (!real)
		return std::nullopt; // complex atan, asin, acos, ...: not done yet
	if (n == "asinh") // ln(x + sqrt(x^2 + 1))
		return creal(f.ln(z.re + f.sqrt(f.mul(z.re, z.re) + f.one)));
	if (n == "acosh") { // ln(x + sqrt(x^2 - 1)), x >= 1
		if (z.re.compare(f.one) < 0)
			return std::nullopt;
		return creal(f.ln(z.re + f.sqrt(f.mul(z.re, z.re) - f.one)));
	}
	if (n == "atanh") { // ln((1 + x)/(1 - x))/2, |x| < 1
		if (z.re.abs().compare(f.one) >= 0)
			return std::nullopt;
		return creal(divRound(f.ln(f.div(f.one + z.re, f.one - z.re)), BigInt(2L)));
	}
	if (n == "atan")
		return creal(f.atan(z.re));
	if (n == "asin" || n == "acos") {
		if (z.re.abs().compare(f.one) > 0)
			return std::nullopt; // complex result: not done yet
		BigInt halfPi = divRound(f.pi(), BigInt(2L));
		BigInt as;
		if (z.re.abs().compare(f.one) == 0)
			as = z.re.sign() > 0 ? halfPi : -halfPi;
		else
			as = f.atan(f.div(z.re, f.sqrt(f.one - f.mul(z.re, z.re))));
		return creal(n == "asin" ? as : halfPi - as);
	}
	return std::nullopt;
}

std::optional<Cx> approx(const Expr& e, const Fixed& f)
{
	switch (e->kind) {
	case Kind::Num:
		return creal(f.from(e->num));
	case Kind::Sym:
		if (isImUnit(e))
			return Cx{BigInt(0L), f.one};
		if (e->name == "pi")
			return creal(f.pi());
		return std::nullopt;
	case Kind::Add: {
		Cx s{BigInt(0L), BigInt(0L)};
		for (const Expr& t : e->args) {
			auto v = approx(t, f);
			if (!v)
				return std::nullopt;
			s = {s.re + v->re, s.im + v->im};
		}
		return s;
	}
	case Kind::Mul: {
		Cx p{f.one, BigInt(0L)};
		for (const Expr& t : e->args) {
			auto v = approx(t, f);
			if (!v)
				return std::nullopt;
			p = f.cmul(p, *v);
		}
		return p;
	}
	case Kind::Pow: {
		auto z = approx(e->args[0], f);
		if (!z)
			return std::nullopt;
		long k;
		if (isInt(e->args[1], k)) { // repeated squaring
			Cx base = *z, r{f.one, BigInt(0L)};
			if (k < 0) {
				base = f.cdiv(r, base);
				k = -k;
			}
			while (k > 0) {
				if (k & 1)
					r = f.cmul(r, base);
				k >>= 1;
				if (k)
					base = f.cmul(base, base);
			}
			return r;
		}
		auto w = approx(e->args[1], f);
		if (!w)
			return std::nullopt;
		if (z->re.isZero() && z->im.isZero()) {
			if (w->re.sign() > 0)
				return creal(BigInt(0L));
			throw Error("division by zero");
		}
		return f.cexp(f.cmul(*w, f.cln(*z))); // z^w = exp(w*ln(z))
	}
	case Kind::Call:
		return approxCall(e, f);
	default:
		return std::nullopt;
	}
}

// v (at scale s) rounded or truncated to d decimals, as text
std::string format(const BigInt& v, size_t s, size_t d)
{
	BigInt q = g_truncate ? v.divTrunc(pow10(s - d)) : divRound(v, pow10(s - d));
	std::string digits = q.abs().toString();
	if (digits.size() <= d)
		digits = std::string(d + 1 - digits.size(), '0') + digits;
	std::string r = q.sign() < 0 ? "-" : "";
	r += digits.substr(0, digits.size() - d);
	if (d > 0)
		r += "." + digits.substr(digits.size() - d);
	return r;
}

bool isZeroText(const std::string& t)
{
	for (char c : t)
		if (c != '0' && c != '.' && c != '-')
			return false;
	return true;
}

// a + b*im as text
std::string complexText(std::string re, std::string im)
{
	if (isZeroText(im))
		return isZeroText(re) ? (re[0] == '-' ? re.substr(1) : re) : re;
	std::string unit = imagName();
	bool negative = im[0] == '-';
	std::string mag = negative ? im.substr(1) : im;
	std::string i = (mag == "1" ? "" : mag + "*") + unit;
	if (isZeroText(re))
		return (negative ? "-" : "") + i;
	return re + (negative ? "-" : "+") + i;
}

// exact rational rounded to d decimals
std::string exactText(const Rational& r, size_t d)
{
	BigInt scaled = r.numerator() * pow10(d);
	BigInt q = g_truncate ? scaled.divTrunc(r.denominator()) : divRound(scaled, r.denominator());
	return format(q, d, d);
}

bool hasFreeSymbol(const Expr& e)
{
	if (e->kind == Kind::Sym)
		return !isImUnit(e) && e->name != "pi";
	for (const Expr& a : e->args)
		if (hasFreeSymbol(a))
			return true;
	return false;
}

std::optional<std::string> numberText(const Expr& e, size_t d)
{
	Rational re, im;
	if (complexParts(e, re, im)) { // exact: integers stay, fractions are rounded
		if (re.isInt() && im.isInt())
			return complexText(re.toString(), im.toString());
		return complexText(exactText(re, d), exactText(im, d));
	}
	Fixed f(d + 20);
	auto v = approx(e, f);
	if (!v)
		return std::nullopt;
	return complexText(format(v->re, f.S, d), format(v->im, f.S, d));
}

// numbers to decimals, symbols kept: x/3 + sqrt(2) -> 0.333*x + 1.414
Expr numify(const Expr& e, size_t d)
{
	if (!hasFreeSymbol(e)) {
		auto t = numberText(e, d);
		return t ? mkDec(*t) : e; // e.g. asin(2): not done yet, kept
	}
	switch (e->kind) {
	case Kind::Add: {
		Exprs a;
		for (const Expr& t : e->args)
			a.push_back(numify(t, d));
		return mkNode(Kind::Add, a);
	}
	case Kind::Mul: {
		// the numeric factors become one decimal in front: 0.841*y
		Exprs constant, a;
		for (const Expr& f : e->args)
			if (!hasFreeSymbol(f))
				constant.push_back(f);
		if (!constant.empty()) {
			Expr c = simpMul(constant);
			auto t = numberText(c, d);
			if (c->kind == Kind::Num && c->num.isInt()) {
				a.push_back(c); // integers stay: 2*x
			} else if (!t) {
				a.push_back(c);
			} else if ((*t)[0] == '-' && t->find_first_of("+-", 1) == std::string::npos) {
				a.push_back(mkInt(-1)); // -0.333*x, not (-0.333)*x
				a.push_back(mkDec(t->substr(1)));
			} else {
				a.push_back(mkDec(*t));
			}
		}
		for (const Expr& f : e->args)
			if (hasFreeSymbol(f))
				a.push_back(numify(f, d));
		return mkNode(Kind::Mul, a);
	}
	case Kind::Pow: // the exponent stays exact: x^2, x^(1/2)
		return mkPow(numify(e->args[0], d), e->args[1]);
	case Kind::Call: {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(numify(x, d));
		return mkCall(e->name, a);
	}
	default:
		return e;
	}
}

} // namespace

void setNumTruncate(bool t) { g_truncate = t; }

void clearNumericCaches()
{
	g_pi.clear();
	g_ln2.clear();
}

Expr numeric(const Expr& e, size_t digits)
{
	if (e->kind == Kind::Num && e->num.isInt())
		return e;
	return numify(e, digits);
}

} // namespace xyz
