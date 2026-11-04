// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Converting expressions to text, e.g. x^2+4*x+4, (x+1)^2/(2*y), -x/3.
// The output can be read back by the parser.
#include "expr.hpp"

#include <cctype>

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
	case Kind::Str:
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
		out += isImUnit(n) ? imagName() : n->name;
		break;
	case Kind::Dec:
		out += n->name;
		break;
	case Kind::Str:
		out += '"' + n->name + '"';
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

// ------------------------------------------------------------------ LaTeX

namespace {

const char* const kGreek[] = {
	"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
	"iota", "kappa", "lambda", "mu", "nu", "xi", "pi", "rho", "sigma", "tau",
	"upsilon", "phi", "chi", "psi", "omega", "Gamma", "Delta", "Theta",
	"Lambda", "Xi", "Pi", "Sigma", "Upsilon", "Phi", "Psi", "Omega"};

bool isGreek(const std::string& s)
{
	for (const char* g : kGreek)
		if (s == g)
			return true;
	return false;
}

void lx(std::string& out, const Expr& n, int minPrec);

// joins factors: juxtaposition, or \cdot before a factor starting with a digit
void lxFactors(std::string& out, const Exprs& v)
{
	for (size_t i = 0; i < v.size(); i++) {
		std::string f;
		lx(f, v[i], P_POW);
		if (i)
			out += (!f.empty() && (std::isdigit(static_cast<unsigned char>(f[0])) ||
			                       f.rfind("\\frac", 0) == 0))
			           ? " \\cdot " : " ";
		out += f;
	}
}

void lxProduct(std::string& out, const Expr& n)
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
	if (den.empty()) {
		lxFactors(out, num);
		return;
	}
	out += "\\frac{";
	if (num.empty())
		out += '1';
	else
		lxFactors(out, num);
	out += "}{";
	lxFactors(out, den);
	out += '}';
}

void lxSum(std::string& out, const Expr& n, bool& first)
{
	if (n->kind == Kind::Add) {
		for (const Expr& t : n->args)
			lxSum(out, t, first);
		return;
	}
	std::string t;
	lx(t, n, P_NEG);
	if (!first && t[0] != '-')
		out += '+';
	out += t;
	first = false;
}

void lxArgs(std::string& out, const Exprs& args)
{
	for (size_t i = 0; i < args.size(); i++) {
		if (i)
			out += ", ";
		lx(out, args[i], P_ARG);
	}
}

void lx(std::string& out, const Expr& n, int minPrec)
{
	bool paren = prec(n) < minPrec;
	if (paren)
		out += "\\left(";
	switch (n->kind) {
	case Kind::Num:
		if (n->num.isInt()) {
			out += n->num.toString();
		} else {
			if (n->num.sign() < 0)
				out += '-';
			Rational a = n->num.abs();
			out += "\\frac{" + a.numerator().toString() + "}{" +
			       a.denominator().toString() + "}";
		}
		break;
	case Kind::Sym:
		out += isImUnit(n) ? "i" : latexName(n->name);
		break;
	case Kind::Dec: {
		std::string s = n->name, unit = "*" + imagName();
		size_t p = s.find(unit);
		if (p != std::string::npos)
			s.replace(p, unit.size(), "\\,i");
		else if (s.size() >= imagName().size() &&
		         s.compare(s.size() - imagName().size(), std::string::npos, imagName()) == 0)
			s.replace(s.size() - imagName().size(), std::string::npos, "i");
		out += s;
		break;
	}
	case Kind::Str:
		out += "\\texttt{" + latexText(n->name) + "}";
		break;
	case Kind::Add: {
		bool first = true;
		lxSum(out, n, first);
		break;
	}
	case Kind::Mul:
		lxProduct(out, n);
		break;
	case Kind::Pow: {
		if (negNumExp(n)) {
			lxProduct(out, n);
			break;
		}
		const Expr& e = n->args[1];
		long k;
		if (e->kind == Kind::Num && e->num.numerator().isOne() &&
		    e->num.denominator().toLong(k) && k >= 2) {
			out += k == 2 ? "\\sqrt{" : "\\sqrt[" + std::to_string(k) + "]{";
			lx(out, n->args[0], P_ARG);
			out += '}';
			break;
		}
		lx(out, n->args[0], P_ATOM);
		out += "^{";
		lx(out, e, P_ARG);
		out += '}';
		break;
	}
	case Kind::Call: {
		const std::string& f = n->name;
		if (int op = opPrec(f); op >= 0) {
			if (f == "!") {
				out += "\\lnot ";
				lx(out, n->args[0], P_ATOM);
				break;
			}
			const char* sym = f == "==" ? "=" : f == "!=" ? "\\neq" : f == "<=" ? "\\le"
			                : f == ">=" ? "\\ge" : f == "&&" ? "\\land"
			                : f == "||" ? "\\lor" : f.c_str();
			lx(out, n->args[0], op == P_REL ? P_ADD : op);
			out += std::string(" ") + sym + " ";
			lx(out, n->args[1], op == P_REL ? P_ADD : op + 1);
			break;
		}
		if (f == "abs" && n->args.size() == 1) {
			out += "\\left|";
			lx(out, n->args[0], P_ARG);
			out += "\\right|";
			break;
		}
		if (f == "conj" && n->args.size() == 1) {
			out += "\\overline{";
			lx(out, n->args[0], P_ARG);
			out += '}';
			break;
		}
		if (f == "Re" || f == "Im" || f.size() > 1)
			out += "\\operatorname{" + latexText(f) + "}";
		else
			out += f;
		out += "\\left(";
		lxArgs(out, n->args);
		out += "\\right)";
		break;
	}
	case Kind::Arg:
		out += latexName(n->name) + " = ";
		lx(out, n->args[0], P_OR);
		if (isRange(n)) {
			out += " \\ldots ";
			lx(out, n->args[1], P_ADD);
		}
		break;
	}
	if (paren)
		out += "\\right)";
}

} // namespace

std::string latexText(const std::string& text)
{
	std::string r;
	for (char c : text) {
		switch (c) {
		case '\\': r += "\\textbackslash{}"; break;
		case '{': case '}': case '$': case '&': case '#': case '_': case '%':
			r += '\\';
			r += c;
			break;
		case '^': r += "\\^{}"; break;
		case '~': r += "\\~{}"; break;
		default: r += c;
		}
	}
	return r;
}

std::string latexName(const std::string& name)
{
	// x_1 -> x_{1}, alpha -> \alpha, abc -> \mathrm{abc}
	size_t u = name.rfind('_');
	if (u != std::string::npos && u > 0 && u + 1 < name.size())
		return latexName(name.substr(0, u)) + "_{" + latexName(name.substr(u + 1)) + "}";
	if (isGreek(name))
		return "\\" + name;
	if (name.size() == 1)
		return name;
	return "\\mathrm{" + latexText(name) + "}";
}

std::string toLatex(const Expr& e)
{
	std::string s;
	lx(s, e, P_ARG);
	return s;
}

} // namespace xyz
