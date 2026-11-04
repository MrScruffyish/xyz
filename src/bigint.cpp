// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
#include "bigint.hpp"

#include <climits>
#include <cstdlib>
#include <memory>
#include <new>

namespace xyz {

namespace {
BclContext g_ctx = nullptr;
// constants compared against all the time (creating them is not free)
BclNumber g_zero, g_one;

struct CFree {
	void operator()(char* p) const { std::free(p); }
};
using CString = std::unique_ptr<char, CFree>;
} // namespace

BclSession::BclSession()
{
	if (bcl_start() != BCL_ERROR_NONE || bcl_init() != BCL_ERROR_NONE)
		throw std::runtime_error("cannot initialise bcl");
	bcl_setAbortOnFatalError(false);
	bcl_setLeadingZeroes(true);
	ctx_ = bcl_ctxt_create();
	if (ctx_ == nullptr || bcl_pushContext(ctx_) != BCL_ERROR_NONE)
		throw std::runtime_error("cannot create bcl context");
	bcl_ctxt_setScale(ctx_, 0); // integer arithmetic
	g_ctx = ctx_;
	g_zero = bcl_bigdig2num(0);
	g_one = bcl_bigdig2num(1);
	if (bcl_err(g_zero) != BCL_ERROR_NONE || bcl_err(g_one) != BCL_ERROR_NONE)
		throw std::runtime_error("cannot create bcl constants");
}

BclSession::~BclSession()
{
	bcl_num_free(g_zero);
	bcl_num_free(g_one);
	bcl_gc();
	bcl_popContext();
	bcl_ctxt_free(ctx_);
	bcl_free();
	bcl_end();
	g_ctx = nullptr;
}

void BclSession::gc() { bcl_gc(); }

BclNumber BigInt::check(BclNumber n)
{
	BclError e = bcl_err(n);
	if (e == BCL_ERROR_NONE)
		return n;
	if (e == BCL_ERROR_FATAL_ALLOC_ERR)
		throw std::bad_alloc();
	if (e == BCL_ERROR_SIGNAL)
		throw Error("interrupted");
	if (e == BCL_ERROR_MATH_DIVIDE_BY_ZERO)
		throw Error("division by zero");
	if (e == BCL_ERROR_MATH_OVERFLOW)
		throw Error("number too large");
	throw std::runtime_error("bcl: internal error " + std::to_string(int(e)));
}

BigInt::BigInt(long v)
{
	unsigned long u = v < 0 ? 0UL - static_cast<unsigned long>(v)
	                        : static_cast<unsigned long>(v);
	n_ = check(bcl_bigdig2num(static_cast<BclBigDig>(u)));
	valid_ = true;
	if (v < 0)
		bcl_num_setNeg(n_, true);
}

BigInt BigInt::fromDigits(const std::string& digits)
{
	return BigInt(check(bcl_parse(digits.empty() ? "0" : digits.c_str())), Raw{});
}

BigInt::BigInt(const BigInt& o) : n_(check(bcl_dup(o.n_))), valid_(true) {}

BigInt& BigInt::operator=(const BigInt& o)
{
	if (this != &o) {
		BigInt t(o);
		*this = std::move(t);
	}
	return *this;
}

BigInt& BigInt::operator=(BigInt&& o) noexcept
{
	if (this != &o) {
		if (valid_)
			bcl_num_free(n_);
		n_ = o.n_;
		valid_ = o.valid_;
		o.valid_ = false;
	}
	return *this;
}

BigInt::~BigInt()
{
	if (valid_ && g_ctx != nullptr)
		bcl_num_free(n_);
}

BigInt operator+(const BigInt& a, const BigInt& b)
{
	return BigInt(BigInt::check(bcl_add_keep(a.n_, b.n_)), BigInt::Raw{});
}

BigInt operator-(const BigInt& a, const BigInt& b)
{
	return BigInt(BigInt::check(bcl_sub_keep(a.n_, b.n_)), BigInt::Raw{});
}

BigInt operator*(const BigInt& a, const BigInt& b)
{
	return BigInt(BigInt::check(bcl_mul_keep(a.n_, b.n_)), BigInt::Raw{});
}

BigInt BigInt::operator-() const
{
	BigInt r(*this);
	if (!r.isZero())
		bcl_num_setNeg(r.n_, !bcl_num_neg(r.n_));
	return r;
}

BigInt BigInt::divTrunc(const BigInt& b) const
{
	return BigInt(check(bcl_div_keep(n_, b.n_)), Raw{});
}

BigInt BigInt::mod(const BigInt& b) const
{
	return BigInt(check(bcl_mod_keep(n_, b.n_)), Raw{});
}

BigInt BigInt::pow(unsigned long e) const
{
	BigInt be(check(bcl_bigdig2num(static_cast<BclBigDig>(e))), Raw{});
	return BigInt(check(bcl_pow_keep(n_, be.n_)), Raw{});
}

BigInt BigInt::abs() const
{
	BigInt r(*this);
	bcl_num_setNeg(r.n_, false);
	return r;
}

BigInt BigInt::gcd(const BigInt& a, const BigInt& b)
{
	BigInt x = a.abs(), y = b.abs();
	while (!y.isZero()) {
		BigInt t = x.mod(y);
		x = std::move(y);
		y = std::move(t);
	}
	return x;
}

bool BigInt::isOne() const { return bcl_cmp(n_, g_one) == 0; }

int BigInt::compare(const BigInt& b) const
{
	ssize_t c = bcl_cmp(n_, b.n_);
	return c < 0 ? -1 : c > 0 ? 1 : 0;
}

int BigInt::sign() const
{
	ssize_t c = bcl_cmp(n_, g_zero);
	return c < 0 ? -1 : c > 0 ? 1 : 0;
}

bool BigInt::toLong(long& out) const
{
	BclBigDig v;
	BigInt m = abs();
	if (bcl_bigdig_keep(m.n_, &v) != BCL_ERROR_NONE)
		return false;
	if (v > static_cast<BclBigDig>(LONG_MAX))
		return false;
	out = sign() < 0 ? -static_cast<long>(v) : static_cast<long>(v);
	return true;
}

std::string BigInt::toString() const
{
	CString s(bcl_string_keep(n_));
	if (!s)
		throw std::bad_alloc();
	return std::string(s.get());
}

std::string BigInt::divDecimal(const BigInt& a, const BigInt& b, size_t digits)
{
	bcl_ctxt_setScale(g_ctx, digits);
	BclNumber q = bcl_div_keep(a.n_, b.n_);
	bcl_ctxt_setScale(g_ctx, 0);
	BigInt r(check(q), Raw{});
	return r.toString();
}

} // namespace xyz
