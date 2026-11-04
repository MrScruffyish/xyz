// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
#include "rational.hpp"

#include <utility>

namespace xyz {

Rational::Rational(BigInt n, BigInt d) : n_(std::move(n)), d_(std::move(d))
{
	if (d_.isZero())
		throw Error("division by zero");
	if (d_.sign() < 0) {
		n_ = -n_;
		d_ = -d_;
	}
	if (n_.isZero()) {
		d_ = BigInt(1L);
		return;
	}
	if (d_.isOne())
		return;
	BigInt g = BigInt::gcd(n_, d_);
	if (!g.isOne()) {
		n_ = n_.divTrunc(g);
		d_ = d_.divTrunc(g);
	}
}

Rational Rational::fromDecimal(const std::string& s)
{
	std::string digits;
	unsigned long frac = 0;
	bool after = false;
	for (char c : s) {
		if (c == '.') {
			after = true;
			continue;
		}
		digits += c;
		if (after)
			frac++;
	}
	return Rational(BigInt::fromDigits(digits), BigInt(10L).pow(frac));
}

Rational operator+(const Rational& a, const Rational& b)
{
	if (a.isInt() && b.isInt())
		return Rational(a.n_ + b.n_, BigInt(1L));
	return Rational(a.n_ * b.d_ + b.n_ * a.d_, a.d_ * b.d_);
}

Rational operator-(const Rational& a, const Rational& b) { return a + (-b); }

Rational operator*(const Rational& a, const Rational& b)
{
	if (a.isInt() && b.isInt())
		return Rational(a.n_ * b.n_, BigInt(1L));
	return Rational(a.n_ * b.n_, a.d_ * b.d_);
}

Rational operator/(const Rational& a, const Rational& b)
{
	if (b.isZero())
		throw Error("division by zero");
	return Rational(a.n_ * b.d_, a.d_ * b.n_);
}

Rational Rational::operator-() const
{
	Rational r(*this);
	r.n_ = -r.n_;
	return r;
}

Rational Rational::abs() const
{
	Rational r(*this);
	r.n_ = r.n_.abs();
	return r;
}

Rational Rational::pow(long e) const
{
	if (e == 0)
		return Rational(1);
	if (e < 0 && isZero())
		throw Error("division by zero");
	unsigned long u = e < 0 ? 0UL - static_cast<unsigned long>(e)
	                        : static_cast<unsigned long>(e);
	BigInt p = n_.pow(u), q = d_.pow(u);
	return e < 0 ? Rational(std::move(q), std::move(p))
	             : Rational(std::move(p), std::move(q));
}

int Rational::compare(const Rational& b) const
{
	if (isInt() && b.isInt())
		return n_.compare(b.n_);
	return (n_ * b.d_).compare(b.n_ * d_);
}

std::string Rational::toString() const
{
	if (isInt())
		return n_.toString();
	return n_.toString() + "/" + d_.toString();
}

std::string Rational::toDecimal(size_t digits) const
{
	if (isInt())
		return n_.toString();
	return BigInt::divDecimal(n_, d_, digits);
}

} // namespace xyz
