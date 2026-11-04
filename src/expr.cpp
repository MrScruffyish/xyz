// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
#include "expr.hpp"

namespace xyz {

namespace {
Expr make(Kind k, Rational num, std::string name, Exprs args)
{
	return std::make_shared<const Node>(
		Node{k, std::move(num), std::move(name), std::move(args)});
}
} // namespace

const char* const kImag = "%i";

namespace {
std::string g_imagName = "im";
}

const std::string& imagName() { return g_imagName; }
void setImagName(const std::string& name) { g_imagName = name; }

Expr mkNum(Rational r) { return make(Kind::Num, std::move(r), {}, {}); }
Expr mkInt(long v) { return mkNum(Rational(v)); }
Expr mkSym(std::string name) { return make(Kind::Sym, {}, std::move(name), {}); }
Expr mkDec(std::string text) { return make(Kind::Dec, {}, std::move(text), {}); }
Expr mkStr(std::string text) { return make(Kind::Str, {}, std::move(text), {}); }
Expr mkNode(Kind k, Exprs args) { return make(k, {}, {}, std::move(args)); }
Expr mkPow(Expr b, Expr e) { return mkNode(Kind::Pow, {std::move(b), std::move(e)}); }

Expr mkCall(std::string name, Exprs args)
{
	return make(Kind::Call, {}, std::move(name), std::move(args));
}

Expr mkArg(std::string name, Expr value)
{
	return make(Kind::Arg, {}, std::move(name), {std::move(value)});
}

Expr mkRange(std::string name, Expr start, Expr stop, Expr step)
{
	return make(Kind::Arg, {}, std::move(name),
	            {std::move(start), std::move(stop), std::move(step)});
}

Expr rawAdd(Expr a, Expr b) { return mkNode(Kind::Add, {std::move(a), std::move(b)}); }
Expr rawMul(Expr a, Expr b) { return mkNode(Kind::Mul, {std::move(a), std::move(b)}); }
Expr rawNeg(Expr a) { return rawMul(mkInt(-1), std::move(a)); }
Expr rawSub(Expr a, Expr b) { return rawAdd(std::move(a), rawNeg(std::move(b))); }
Expr rawDiv(Expr a, Expr b) { return rawMul(std::move(a), mkPow(std::move(b), mkInt(-1))); }

namespace {
int rank(Kind k)
{
	switch (k) {
	case Kind::Num: return 0;
	case Kind::Sym: return 1;
	case Kind::Pow: return 2;
	case Kind::Mul: return 3;
	case Kind::Add: return 4;
	case Kind::Call: return 5;
	case Kind::Arg: return 6;
	case Kind::Dec: return 7;
	case Kind::Str: return 8;
	}
	return 9;
}
} // namespace

int compare(const Expr& a, const Expr& b)
{
	if (a == b)
		return 0;
	int ra = rank(a->kind), rb = rank(b->kind);
	if (ra != rb)
		return ra < rb ? -1 : 1;
	if (a->kind == Kind::Num)
		return a->num.compare(b->num);
	if (int c = a->name.compare(b->name))
		return c < 0 ? -1 : 1;
	for (size_t i = 0; i < a->args.size() && i < b->args.size(); i++)
		if (int c = compare(a->args[i], b->args[i]))
			return c;
	if (a->args.size() != b->args.size())
		return a->args.size() < b->args.size() ? -1 : 1;
	return 0;
}

bool isInt(const Expr& e, long& v) { return e->kind == Kind::Num && e->num.toLong(v); }

bool isNumber(const Expr& e, long v)
{
	long x;
	return isInt(e, x) && x == v;
}

bool isZero(const Expr& e) { return e->kind == Kind::Num && e->num.isZero(); }

bool containsSym(const Expr& e, const std::string& name)
{
	if (e->kind == Kind::Sym)
		return e->name == name;
	for (const Expr& a : e->args)
		if (containsSym(a, name))
			return true;
	return false;
}

} // namespace xyz
