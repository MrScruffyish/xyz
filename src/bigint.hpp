// xyz - a small computer algebra system on top of bcl (bc's math library).
// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// BigInt: an RAII wrapper around a bcl number used as an arbitrary
// precision integer (bcl scale 0). All arithmetic is done by bcl.
#pragma once

#include <stdexcept>
#include <string>
#include <sys/types.h>

// bcl.h is a C header that uses the C99 'restrict' keyword.
extern "C" {
#define restrict __restrict
#include <bcl.h>
#undef restrict
}

namespace xyz {

// A user-level error (bad input, division by zero, ...). The interpreter
// reports it and continues with the next statement.
struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

// Initialises bcl and owns the bcl context. Exactly one must exist while
// any BigInt is alive.
class BclSession {
public:
	BclSession();
	~BclSession();
	BclSession(const BclSession&) = delete;
	BclSession& operator=(const BclSession&) = delete;
	static void gc(); // release bcl's cache of free numbers

private:
	BclContext ctx_;
};

class BigInt {
public:
	BigInt() : BigInt(0L) {}
	BigInt(long v); // NOLINT(google-explicit-constructor)
	static BigInt fromDigits(const std::string& digits); // decimal digits

	BigInt(const BigInt& o);
	BigInt(BigInt&& o) noexcept : n_(o.n_), valid_(o.valid_) { o.valid_ = false; }
	BigInt& operator=(const BigInt& o);
	BigInt& operator=(BigInt&& o) noexcept;
	~BigInt();

	friend BigInt operator+(const BigInt& a, const BigInt& b);
	friend BigInt operator-(const BigInt& a, const BigInt& b);
	friend BigInt operator*(const BigInt& a, const BigInt& b);
	BigInt operator-() const;
	BigInt divTrunc(const BigInt& b) const; // exact when b divides *this
	BigInt mod(const BigInt& b) const;
	BigInt pow(unsigned long e) const;
	BigInt abs() const;
	static BigInt gcd(const BigInt& a, const BigInt& b); // >= 0

	int compare(const BigInt& b) const;
	int sign() const;
	bool isZero() const { return sign() == 0; }
	bool isOne() const;
	bool toLong(long& out) const; // false if it does not fit

	std::string toString() const;
	// a/b truncated to 'digits' decimal places, formatted by bcl
	static std::string divDecimal(const BigInt& a, const BigInt& b, size_t digits);

private:
	struct Raw {};
	BigInt(BclNumber n, Raw) : n_(n), valid_(true) {}
	static BclNumber check(BclNumber n);
	BclNumber n_{};
	bool valid_ = false;
};

inline bool operator==(const BigInt& a, const BigInt& b) { return a.compare(b) == 0; }
inline bool operator!=(const BigInt& a, const BigInt& b) { return a.compare(b) != 0; }

} // namespace xyz
