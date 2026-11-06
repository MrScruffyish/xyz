// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Assumptions about symbols (assume(x > 0), assume(n, integer)) and what
// follows from them: the known sign of an expression, and whether it is an
// integer. All symbols are real; assumptions add a sign or integrality.
#include "expr.hpp"

#include <map>

namespace xyz {

namespace {

struct Assumed {
	Sign sign = Sign::Unknown;
	bool integer = false;
};

std::map<std::string, Assumed> g_assumed;

// sign as (s, strict): s in {-1, 0, 1}; strict false means s or 0
struct S {
	int s;
	bool strict;
	bool known;
};

S toS(Sign g)
{
	switch (g) {
	case Sign::Pos: return {1, true, true};
	case Sign::NonNeg: return {1, false, true};
	case Sign::Neg: return {-1, true, true};
	case Sign::NonPos: return {-1, false, true};
	case Sign::Zero: return {0, true, true};
	default: return {0, false, false};
	}
}

Sign fromS(S v)
{
	if (!v.known)
		return Sign::Unknown;
	if (v.s == 0)
		return Sign::Zero;
	if (v.s > 0)
		return v.strict ? Sign::Pos : Sign::NonNeg;
	return v.strict ? Sign::Neg : Sign::NonPos;
}

bool isRealValued(const Expr& e) { return !containsSym(e, kImag); }

} // namespace

void setAssumption(const std::string& x, Sign s, bool integer)
{
	Assumed& a = g_assumed[x];
	if (s != Sign::Unknown)
		a.sign = s;
	a.integer = a.integer || integer;
}

void forgetAssumption(const std::string& x) { g_assumed.erase(x); }

std::vector<std::string> listAssumptions()
{
	std::vector<std::string> out;
	for (const auto& [x, a] : g_assumed) {
		const char* rel = a.sign == Sign::Pos ? " > 0" : a.sign == Sign::NonNeg ? " >= 0"
		                : a.sign == Sign::Neg ? " < 0" : a.sign == Sign::NonPos ? " <= 0"
		                : a.sign == Sign::NonZero ? " != 0" : nullptr;
		if (rel)
			out.push_back(x + rel);
		if (a.integer)
			out.push_back(x + " is an integer");
	}
	return out;
}

bool hasAssumptions(const std::string& x) { return g_assumed.count(x) != 0; }

Sign signOf(const Expr& e)
{
	switch (e->kind) {
	case Kind::Num:
		return e->num.sign() > 0 ? Sign::Pos : e->num.sign() < 0 ? Sign::Neg : Sign::Zero;
	case Kind::Sym: {
		if (e->name == "pi")
			return Sign::Pos;
		auto it = g_assumed.find(e->name);
		if (it == g_assumed.end() || it->second.sign == Sign::NonZero)
			return Sign::Unknown; // nonzero: the sign is still unknown
		return it->second.sign;
	}
	case Kind::Mul: {
		S r{1, true, true};
		for (const Expr& f : e->args) {
			S v = toS(signOf(f));
			if (!v.known)
				return Sign::Unknown;
			if (v.s == 0 && v.strict)
				return Sign::Zero;
			r.s *= v.s == 0 ? 1 : v.s;
			r.strict = r.strict && v.strict;
		}
		return fromS(r);
	}
	case Kind::Add: {
		int dir = 0;
		bool strict = false;
		for (const Expr& t : e->args) {
			S v = toS(signOf(t));
			if (!v.known)
				return Sign::Unknown;
			if (v.s == 0)
				continue;
			if (dir != 0 && v.s != dir)
				return Sign::Unknown;
			dir = v.s;
			strict = strict || v.strict;
		}
		if (dir == 0)
			return Sign::Zero;
		return fromS({dir, strict, true});
	}
	case Kind::Pow: {
		const Expr& b = e->args[0];
		const Expr& x = e->args[1];
		S sb = toS(signOf(b));
		long k;
		if (isInt(x, k)) {
			if (k % 2 == 0 && isRealValued(b)) // even power of a real number
				return sb.known && sb.s != 0 ? Sign::Pos : Sign::NonNeg;
			return sb.known ? fromS(sb) : Sign::Unknown;
		}
		if (sb.known && sb.s > 0 && isRealValued(x))
			return fromS(sb);
		return Sign::Unknown;
	}
	case Kind::Call: {
		const std::string& n = e->name;
		if (e->args.empty())
			return Sign::Unknown;
		const Expr& u = e->args[0];
		if ((n == "exp" || n == "cosh") && isRealValued(u))
			return Sign::Pos;
		if (n == "abs")
			return Sign::NonNeg;
		if (n == "sinh" || n == "tanh" || n == "atan" || n == "asinh")
			return signOf(u);
		return Sign::Unknown;
	}
	default:
		return Sign::Unknown;
	}
}

bool nonNegative(const Expr& e)
{
	Sign s = signOf(e);
	return s == Sign::Pos || s == Sign::NonNeg || s == Sign::Zero;
}

bool positive(const Expr& e) { return signOf(e) == Sign::Pos; }

bool hasSignAssumption(const std::string& x)
{
	auto it = g_assumed.find(x);
	return it != g_assumed.end() && it->second.sign != Sign::Unknown;
}

bool assumedInteger(const std::string& x)
{
	auto it = g_assumed.find(x);
	return it != g_assumed.end() && it->second.integer;
}

bool nonZero(const Expr& e)
{
	Sign s = signOf(e);
	if (s == Sign::Pos || s == Sign::Neg)
		return true;
	switch (e->kind) {
	case Kind::Num:
		return !e->num.isZero();
	case Kind::Sym: {
		if (isImUnit(e))
			return true;
		auto it = g_assumed.find(e->name);
		return it != g_assumed.end() && it->second.sign == Sign::NonZero;
	}
	case Kind::Mul:
		for (const Expr& f : e->args)
			if (!nonZero(f))
				return false;
		return true;
	case Kind::Pow:
		return nonZero(e->args[0]);
	case Kind::Call:
		return e->name == "exp" || e->name == "cosh";
	default:
		return false;
	}
}

bool isIntegerValued(const Expr& e)
{
	long k;
	switch (e->kind) {
	case Kind::Num:
		return e->num.isInt();
	case Kind::Sym: {
		auto it = g_assumed.find(e->name);
		return it != g_assumed.end() && it->second.integer;
	}
	case Kind::Add:
	case Kind::Mul:
		for (const Expr& a : e->args)
			if (!isIntegerValued(a))
				return false;
		return true;
	case Kind::Pow:
		return isIntegerValued(e->args[0]) && isInt(e->args[1], k) && k >= 0;
	default:
		return false;
	}
}

} // namespace xyz
