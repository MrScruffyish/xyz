// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Exact rational numbers p/q built on BigInt (bcl).
// Invariant: q > 0 and gcd(|p|, q) == 1.
#pragma once

#include "bigint.hpp"

#include <string>

namespace xyz {

class Rational {
public:
	Rational(long v = 0) : n_(v), d_(1L) {} // NOLINT(google-explicit-constructor)
	Rational(BigInt n, BigInt d);           // normalises; d != 0
	static Rational fromDecimal(const std::string& s); // "12", "1.25", ".5"

	const BigInt& numerator() const { return n_; }
	const BigInt& denominator() const { return d_; }

	friend Rational operator+(const Rational& a, const Rational& b);
	friend Rational operator-(const Rational& a, const Rational& b);
	friend Rational operator*(const Rational& a, const Rational& b);
	friend Rational operator/(const Rational& a, const Rational& b);
	Rational operator-() const;
	Rational abs() const;
	Rational pow(long e) const; // e may be negative

	int compare(const Rational& b) const;
	int sign() const { return n_.sign(); }
	bool isZero() const { return n_.isZero(); }
	bool isInt() const { return d_.isOne(); }
	bool isOne() const { return isInt() && n_.isOne(); }
	bool toLong(long& out) const { return isInt() && n_.toLong(out); }

	std::string toString() const;                // "p" or "p/q"
	std::string toDecimal(size_t digits) const;  // truncated, like bc

private:
	BigInt n_, d_;
};

} // namespace xyz
