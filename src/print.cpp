// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Converting expressions to text, e.g. x^2+4*x+4, (x+1)^2/(2*y), -x/3.
// The output can be read back by the parser.
#include "expr.hpp"

namespace xyz {

namespace {

enum Prec { P_ARG, P_OR, P_AND, P_NOT, P_REL, P_ADD, P_NEG, P_MUL, P_POW, P_ATOM };

// comparison and logic operators are calls with these names
int opPrec(const std::string& name)
{
	if (name == "||")
		return P_OR;
	if (name == "&&")
		return P_AND;
	if (name == "!")
		return P_NOT;
	if (name == "==" || name == "!=" || name == "<" || name == "<=" ||
	    name == ">" || name == ">=")
		return P_REL;
	return -1;
}

bool negNumExp(const Expr& n)
{
	return n->kind == Kind::Pow && n->args[1]->kind == Kind::Num &&
	       n->args[1]->num.sign() < 0;
}

int prec(const Expr& n)
{
	switch (n->kind) {
	case Kind::Num:
		if (n->num.sign() < 0)
			return P_NEG;
		return n->num.isInt() ? P_ATOM : P_MUL;
	case Kind::Dec:
		return n->name[0] == '-' ? P_NEG : P_ATOM;
	case Kind::Sym:
		return P_ATOM;
	case Kind::Call:
		return opPrec(n->name) >= 0 ? opPrec(n->name) : P_ATOM;
	case Kind::Add:
		return P_ADD;
	case Kind::Mul:
		for (const Expr& f : n->args)
			if (f->kind == Kind::Num && f->num.sign() < 0)
				return P_NEG;
		return P_MUL;
	case Kind::Pow:
		return negNumExp(n) ? P_MUL : P_POW;
	case Kind::Arg:
		return P_ARG;
	}
	return P_ATOM;
}

void pr(std::string& out, const Expr& n, int minPrec);

void flattenMul(const Expr& n, Exprs& out)
{
	if (n->kind == Kind::Mul)
		for (const Expr& f : n->args)
			flattenMul(f, out);
	else
		out.push_back(n);
}

void prList(std::string& out, const Exprs& v)
{
	for (size_t i = 0; i < v.size(); i++) {
		if (i)
			out += '*';
		pr(out, v[i], P_POW);
	}
}

// products (and powers with negative exponent) as  [-]num/den
void prProduct(std::string& out, const Expr& n)
{
	Exprs f;
	flattenMul(n, f);
	Rational c(1);
	bool hasSym = false;
	for (const Expr& x : f) {
		if (x->kind == Kind::Num)
			c = c * x->num;
		else
			hasSym = true;
	}
	if (c.sign() < 0)
		out += '-';
	Rational a = c.abs();
	Exprs num, den;
	if (!(a.numerator().isOne() && hasSym))
		num.push_back(mkNum(Rational(a.numerator(), BigInt(1L))));
	if (!a.denominator().isOne())
		den.push_back(mkNum(Rational(a.denominator(), BigInt(1L))));
	for (const Expr& x : f) {
		if (x->kind == Kind::Num)
			continue;
		if (negNumExp(x)) {
			Rational e = -x->args[1]->num;
			den.push_back(e.isOne() ? x->args[0] : mkPow(x->args[0], mkNum(e)));
		} else {
			num.push_back(x);
		}
	}
	if (num.empty())
		out += '1';
	prList(out, num);
	if (!den.empty()) {
		out += '/';
		if (den.size() == 1) {
			pr(out, den[0], P_POW);
		} else {
			out += '(';
			prList(out, den);
			out += ')';
		}
	}
}

void prSum(std::string& out, const Expr& n, bool& first)
{
	if (n->kind == Kind::Add) {
		for (const Expr& t : n->args)
			prSum(out, t, first);
		return;
	}
	std::string t;
	pr(t, n, P_NEG);
	if (!first && t[0] != '-')
		out += '+';
	out += t;
	first = false;
}

void pr(std::string& out, const Expr& n, int minPrec)
{
	bool paren = prec(n) < minPrec;
	if (paren)
		out += '(';
	switch (n->kind) {
	case Kind::Num:
		out += n->num.toString();
		break;
	case Kind::Sym:
	case Kind::Dec:
		out += n->name;
		break;
	case Kind::Add: {
		bool first = true;
		prSum(out, n, first);
		break;
	}
	case Kind::Mul:
		prProduct(out, n);
		break;
	case Kind::Pow:
		if (negNumExp(n)) {
			prProduct(out, n);
		} else {
			pr(out, n->args[0], P_ATOM);
			out += '^';
			pr(out, n->args[1], P_ATOM);
		}
		break;
	case Kind::Call:
		if (int op = opPrec(n->name); op >= 0) {
			if (n->name == "!") {
				out += '!';
				pr(out, n->args[0], P_ATOM); // !(a == b), not !a == b
			} else {
				// left-associative for && and ||; relations do not chain
				pr(out, n->args[0], op == P_REL ? P_ADD : op);
				out += ' ' + n->name + ' ';
				pr(out, n->args[1], op == P_REL ? P_ADD : op + 1);
			}
			break;
		}
		out += n->name;
		out += '(';
		for (size_t i = 0; i < n->args.size(); i++) {
			if (i)
				out += ", ";
			pr(out, n->args[i], P_ARG);
		}
		out += ')';
		break;
	case Kind::Arg:
		out += n->name;
		out += '=';
		pr(out, n->args[0], P_OR);
		if (isRange(n)) {
			out += " to ";
			pr(out, n->args[1], P_ADD);
			if (!isNumber(n->args[2], 1)) {
				out += " step ";
				pr(out, n->args[2], P_ADD);
			}
		}
		break;
	}
	if (paren)
		out += ')';
}

} // namespace

std::string toString(const Expr& e)
{
	std::string s;
	pr(s, e, P_ARG);
	return s;
}

} // namespace xyz
