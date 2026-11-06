// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
#include "interpreter.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <set>
#include <new>
#include <ostream>

namespace xyz {

namespace {

const int kMaxCallDepth = 200;

struct Builtin {
	const char* name;
	size_t min, max;   // number of arguments
	int varArg;        // index of a "variable name" argument, or -1
	int shadowTo;      // that variable is free in arguments 0..shadowTo
	bool statementOnly;
};

const Builtin builtins[] = {
	{"eval", 1, 1000, -1, -1, false},
	{"subs", 1, 1000, -1, -1, false},
	{"expand", 1, 1, -1, -1, false},
	{"num", 1, 2, -1, -1, false},
	{"diff", 2, 3, 1, 0, false},
	{"deg", 2, 2, 1, 0, false},
	{"coeff", 3, 3, 1, 0, false},
	{"quo", 3, 3, 2, 1, false},
	{"rem", 3, 3, 2, 1, false},
	{"gcd", 1, 1000, -1, -1, false},
	{"lcm", 1, 1000, -1, -1, false},
	{"cancel", 1, 1, -1, -1, false},
	{"Re", 1, 1, -1, -1, false},
	{"Im", 1, 1, -1, -1, false},
	{"conj", 1, 1, -1, -1, false},
	{"abs", 1, 1, -1, -1, false},
	{"sqrt", 1, 1, -1, -1, false},
	{"sin", 1, 1, -1, -1, false},
	{"sinh", 1, 1, -1, -1, false},
	{"cosh", 1, 1, -1, -1, false},
	{"tanh", 1, 1, -1, -1, false},
	{"asinh", 1, 1, -1, -1, false},
	{"acosh", 1, 1, -1, -1, false},
	{"atanh", 1, 1, -1, -1, false},
	{"cos", 1, 1, -1, -1, false},
	{"tan", 1, 1, -1, -1, false},
	{"asin", 1, 1, -1, -1, false},
	{"acos", 1, 1, -1, -1, false},
	{"atan", 1, 1, -1, -1, false},
	{"exp", 1, 1, -1, -1, false},
	{"ln", 1, 1, -1, -1, false},
	{"log", 1, 2, -1, -1, false},
	{"trigexpand", 1, 1, -1, -1, false},
	{"trigreduce", 1, 1, -1, -1, false},
	{"factor", 1, 2, -1, -1, false},
	{"length", 1, 1, -1, -1, false},
	{"integrate", 2, 4, 1, 0, false},
	{"taylor", 2, 3, -1, -1, false},
	{"limit", 2, 3, -1, -1, false},
	{"seq", 2, 2, -1, -1, false},
	{"sum", 1, 2, -1, -1, false},
	{"solve", 1, 3, 1, 0, false},
	{"disp", 1, 1000, -1, -1, true},
	{"def", 1, 1, -1, -1, true},
	{"unset", 1, 1000, -1, -1, true},
	{"table", 1, 1000, -1, -1, true},
	{"save", 1, 1, -1, -1, true},
	{"transcript", 1, 1, -1, -1, true},
	{"latex", 1, 1, -1, -1, true},
	{"assume", 1, 1000, -1, -1, true},
	{"forget", 1, 1000, -1, -1, true},
	{"assumptions", 0, 0, -1, -1, true},
};

const Builtin* findBuiltin(const std::string& name)
{
	for (const Builtin& b : builtins)
		if (name == b.name)
			return &b;
	return nullptr;
}

// names with a fixed meaning that cannot be assigned or used as parameters
void checkNotReserved(const std::string& name)
{
	if (name == imagName())
		throw Error("'" + name + "' is the imaginary unit and cannot be assigned");
	if (name == "pi" || name == "inf")
		throw Error("'" + name + "' is a constant and cannot be assigned");
}

// f applied element by element when some arguments are lists (of the same
// length; the other arguments are used with every element)
Expr broadcast(const Exprs& a, const std::function<Expr(const Exprs&)>& f)
{
	size_t n = 0;
	bool any = false;
	for (const Expr& x : a)
		if (isList(x)) {
			if (any && x->args.size() != n)
				throw Error("lists of different lengths (" + std::to_string(n) + " and " +
				            std::to_string(x->args.size()) + ") cannot be combined");
			n = x->args.size();
			any = true;
		}
	if (!any) {
		Exprs v;
		for (const Expr& x : a)
			v.push_back(heldValue(x));
		return f(v);
	}
	Exprs out;
	for (size_t i = 0; i < n; i++) {
		Exprs ai;
		for (const Expr& x : a)
			ai.push_back(isList(x) ? x->args[i] : x);
		out.push_back(broadcast(ai, f));
	}
	return mkList(out);
}

Expr mapList(const Expr& v, const std::function<Expr(const Expr&)>& f)
{
	return broadcast({v}, [&f](const Exprs& a) { return f(a[0]); });
}

bool listsEqual(const Expr& a, const Expr& b)
{
	if (isList(a) || isList(b)) {
		if (!isList(a) || !isList(b) || a->args.size() != b->args.size())
			return false;
		for (size_t i = 0; i < a->args.size(); i++)
			if (!listsEqual(a->args[i], b->args[i]))
				return false;
		return true;
	}
	return isIdenticallyZero(sub(heldValue(a), heldValue(b)));
}

// integral of f for x from a to b: F(b) - F(a) (no check for poles between)
Expr definiteIntegral(const Expr& f, const std::string& x, const Expr& a, const Expr& b)
{
	// a pole of f between the bounds (a root of its denominator) makes the
	// integral improper: F(b) - F(a) would be wrong
	auto value = [](const Expr& v, double& d) {
		if (v->kind == Kind::Sym && v->name == "inf") {
			d = 1e300;
			return true;
		}
		if (v->kind == Kind::Mul && v->args.size() == 2 && isNumber(v->args[0], -1) &&
		    v->args[1]->kind == Kind::Sym && v->args[1]->name == "inf") {
			d = -1e300;
			return true;
		}
		Expr n = numeric(v, 30);
		if (n->kind == Kind::Num) {
			d = std::stod(toString(numeric(n, 30)));
			return true;
		}
		if (n->kind != Kind::Dec || n->name.find(imagName()) != std::string::npos)
			return false;
		d = std::stod(n->name);
		return true;
	};
	double lo, hi;
	if (value(a, lo) && value(b, hi)) {
		if (lo > hi)
			std::swap(lo, hi);
		Exprs poles;
		try {
			Expr q = cancelParts(f).second;
			if (containsSym(q, x))
				poles = solve(q, x).roots;
		} catch (const Error&) {
		}
		for (const Expr& r : poles) {
			double v;
			if (value(r, v) && v > lo && v < hi)
				throw Error("integrate: " + toString(f) + " has a pole at " + x + " = " +
				            toString(r) + ", between the bounds");
		}
	}
	Expr F = integrate(f, x);
	std::function<bool(const Expr&)> open = [&](const Expr& e) {
		if (e->kind == Kind::Call && e->name == "integrate")
			return true;
		for (const Expr& c : e->args)
			if (open(c))
				return true;
		return false;
	};
	if (open(F))
		return mkCall("integrate", {f, mkSym(x), a, b});
	// F at a bound; a limit where F is not defined there, or at +-inf
	auto at = [&](const Expr& v, int dir) {
		if (!containsSym(v, "inf")) {
			try {
				return substitute(F, x, v);
			} catch (const Error&) {
			}
		}
		return limit(F, x, v, dir);
	};
	Expr fb = at(b, -1), fa = at(a, 1);
	if (containsSym(fb, "inf") || containsSym(fa, "inf")) {
		if (equal(fb, fa))
			throw Error("integrate: inf - inf at the bounds");
		return containsSym(fb, "inf") ? fb : mul(mkInt(-1), fa);
	}
	return sub(fb, fa);
}

// A note about an assumption that would make e simpler, or "": the first
// of abs(x), ln(x*y), ln(x^r), (x^a)^b, sin(n*pi), x/x (kept in a formula)
// whose symbol has no assumption yet
std::string assumptionHint(const Expr& e)
{
	auto freeSym = [](const Expr& u, std::string& name) {
		std::function<bool(const Expr&)> walk = [&](const Expr& n) {
			if (n->kind == Kind::Sym && !isImUnit(n) && n->name != "pi" && n->name != "inf" &&
			    n->name[0] != '~' && !hasSignAssumption(n->name)) {
				name = n->name;
				return true;
			}
			for (const Expr& c : n->args)
				if (walk(c))
					return true;
			return false;
		};
		return walk(u);
	};
	std::string x;
	switch (e->kind) {
	case Kind::Held:
		return assumptionHint(e->args[0]);
	case Kind::Call:
		if (e->name == "abs" && e->args.size() == 1 && freeSym(e->args[0], x))
			return toString(e) + " would be " + toString(e->args[0]) + " with assume(" + x +
			       " >= 0), or " + toString(mul(mkInt(-1), e->args[0])) + " with assume(" + x +
			       " <= 0)";
		if (e->name == "ln" && e->args.size() == 1) {
			const Expr& v = e->args[0];
			if ((v->kind == Kind::Mul || (v->kind == Kind::Pow && v->args[0]->kind == Kind::Sym)) &&
			    freeSym(v, x))
				return "ln(" + toString(v) + ") would split into logarithms with assume(" + x +
				       " > 0)";
		}
		if ((e->name == "sin" || e->name == "cos") && e->args.size() == 1 &&
		    containsSym(e->args[0], "pi")) {
			std::function<bool(const Expr&)> walk = [&](const Expr& n) {
				if (n->kind == Kind::Sym && !isImUnit(n) && n->name != "pi" &&
				    !assumedInteger(n->name)) {
					x = n->name;
					return true;
				}
				for (const Expr& c : n->args)
					if (walk(c))
						return true;
				return false;
			};
			const Expr& u = e->args[0];
			if (u->kind == Kind::Mul && walk(u))
				return e->name + "(" + toString(u) + ") would simplify with assume(" + x +
				       ", integer), if " + x + " is an integer";
		}
		break;
	case Kind::Pow:
		if (e->args[0]->kind == Kind::Pow && e->args[0]->args[0]->kind == Kind::Sym &&
		    freeSym(e->args[0]->args[0], x))
			return toString(e) + " would simplify with assume(" + x + " >= 0)";
		break;
	case Kind::Mul: { // x/x kept in a formula: a base with powers of both signs
		std::vector<std::pair<Expr, int>> seen;
		for (const Expr& f : e->args) {
			Expr b = f;
			int sg = 1;
			if (f->kind == Kind::Pow && f->args[1]->kind == Kind::Num) {
				b = f->args[0];
				sg = f->args[1]->num.sign();
			}
			for (const auto& [b2, s2] : seen)
				if (s2 != sg && equal(b, b2) && freeSym(b, x) && !nonZero(b))
					return toString(b) + "/" + toString(b) + " would be 1 with assume(" + x + " != 0)";
			seen.emplace_back(b, sg);
		}
		break;
	}
	default:
		break;
	}
	for (const Expr& c : e->args) {
		std::string h = assumptionHint(c);
		if (!h.empty())
			return h;
	}
	return "";
}

bool isOperator(const std::string& n)
{
	return n == "==" || n == "!=" || n == "<" || n == "<=" || n == ">" ||
	       n == ">=" || n == "&&" || n == "||" || n == "!";
}

// Is argument i of call c a variable name (never evaluated)?
bool isVarPos(const Expr& c, size_t i)
{
	const Builtin* b = findBuiltin(c->name);
	if (!b)
		return false;
	if (c->name == "def" || c->name == "unset" || c->name == "assume" || c->name == "forget")
		return true;
	return b->varArg >= 0 && static_cast<size_t>(b->varArg) == i;
}

// Is symbol v rebound inside argument i of call c?
bool shadows(const Expr& c, size_t i, const std::string& v)
{
	const Builtin* b = findBuiltin(c->name);
	if (!b)
		return false;
	// eval, subs and table rebind their names inside the first argument
	if ((c->name == "taylor" || c->name == "limit") && i == 0 && c->args.size() > 1) {
		const Expr& a = c->args[1];
		return (a->kind == Kind::Arg || a->kind == Kind::Sym) && a->name == v;
	}
	if (c->name == "eval" || c->name == "subs" || c->name == "table" || c->name == "seq" ||
	    c->name == "sum") {
		if (c->args[i]->kind == Kind::Arg)
			return false;
		for (const Expr& a : c->args)
			if (a->kind == Kind::Arg && a->name == v)
				return true;
		return false;
	}
	if (b->varArg >= 0 && static_cast<size_t>(b->varArg) < c->args.size() &&
	    static_cast<int>(i) <= b->shadowTo) {
		const Expr& x = c->args[static_cast<size_t>(b->varArg)];
		return x->kind == Kind::Sym && x->name == v;
	}
	return false;
}

const std::string& varName(const Expr& c, size_t i)
{
	const Expr& a = c->args[i];
	if (a->kind != Kind::Sym)
		throw Error(c->name + ": argument " + std::to_string(i + 1) +
		            " must be a variable name");
	if (isImUnit(a))
		throw Error(c->name + ": '" + imagName() + "' is the imaginary unit, not a variable");
	if (a->name == "pi")
		throw Error(c->name + ": 'pi' is a constant, not a variable");
	return a->name;
}

void checkStatementOnly(const Expr& e, bool top)
{
	if (e->kind == Kind::Call && !top) {
		const Builtin* b = findBuiltin(e->name);
		if (b && b->statementOnly)
			throw Error(e->name + "() can only be used as a statement on its own");
	}
	for (const Expr& a : e->args)
		checkStatementOnly(a, false);
}

std::string joinParams(const std::vector<std::string>& p)
{
	std::string s;
	for (size_t i = 0; i < p.size(); i++)
		s += (i ? ", " : "") + p[i];
	return s;
}

const Rational& numberOf(const Expr& v, const std::string& what)
{
	if (v->kind != Kind::Num)
		throw Error(what + " must be a number, not " + toString(v));
	return v->num;
}

// Restores the loop-variable stack when a loop body is left (also on errors).
struct LoopGuard {
	std::vector<std::pair<std::string, Expr>>& v;
	explicit LoopGuard(std::vector<std::pair<std::string, Expr>>& s) : v(s) {}
	~LoopGuard() { v.pop_back(); }
};

struct DepthGuard {
	int& d;
	explicit DepthGuard(int& x) : d(x)
	{
		if (++d > kMaxCallDepth) {
			--d;
			throw Error("too many nested function calls (recursive definition?)");
		}
	}
	~DepthGuard() { d--; }
};

} // namespace

// ============================================================ evaluation

Interpreter::Memo* Interpreter::sharedMemo(const Dyn* dyn, bool deep)
{
	return &shared_[{dyn ? dyn->id : 0UL, deep}];
}

Interpreter::Ctx Interpreter::defCtx(const Ctx& c, const std::string* name)
{
	Ctx d;
	d.dyn = c.dyn;
	d.deep = c.deep;
	d.defName = name;
	d.memo = sharedMemo(c.dyn, c.deep);
	return d;
}

Interpreter::Bindings Interpreter::loopBindings() const
{
	Bindings b;
	for (const auto& [name, value] : loopVars_)
		b[name] = value; // innermost wins
	return b;
}

bool Interpreter::isLoopVar(const std::string& name) const
{
	for (const auto& lv : loopVars_)
		if (lv.first == name)
			return true;
	return false;
}

Expr Interpreter::resolve(const Expr& e, const Ctx& c) { return heldValue(resolveKeep(e, c)); }

Expr Interpreter::resolveKeep(const Expr& e, const Ctx& c)
{
	auto hit = c.memo->find(e.get());
	if (hit != c.memo->end())
		return hit->second;
	Expr r;
	switch (e->kind) {
	case Kind::Num:
	case Kind::Dec:
	case Kind::Held:
		return e;
	case Kind::Str:
		throw Error("strings can only be used as file names in save() and "
		            "transcript()");
	case Kind::Sym:
		r = lookup(e, c);
		break;
	case Kind::Add:
	case Kind::Mul: {
		Exprs a;
		a.reserve(e->args.size());
		for (const Expr& x : e->args)
			a.push_back(resolve(x, c));
		bool add = e->kind == Kind::Add;
		r = broadcast(a, [add](const Exprs& v) { return add ? simpAdd(v) : simpMul(v); });
		break;
	}
	case Kind::Pow:
		r = broadcast({resolve(e->args[0], c), resolve(e->args[1], c)},
		              [](const Exprs& v) { return simpPow(v[0], v[1]); });
		break;
	case Kind::Call:
		r = call(e, c);
		break;
	case Kind::Arg:
		throw Error("'" + e->name + "=' is only allowed in the arguments of "
		            "eval() and table()");
	}
	c.memo->emplace(e.get(), r);
	return r;
}

Expr Interpreter::lookup(const Expr& sym, const Ctx& c)
{
	if (isImUnit(sym) || sym->name == "pi" || sym->name == "inf")
		return sym; // constants never have a value
	const std::string& name = sym->name;
	if (c.lex) {
		auto it = c.lex->find(name);
		if (it != c.lex->end())
			return it->second;
	}
	for (const Dyn* d = c.dyn; d; d = d->up) {
		auto it = d->bindings.find(name);
		if (it != d->bindings.end())
			return it->second;
	}
	if (c.inert && !c.deep)
		return sym;
	if (c.defName && isLoopVar(name))
		throw Error(*c.defName + " refers to " + name + ", which is also the loop "
		            "variable here; use eval(" + *c.defName + ", " + name + "=" +
		            name + ") for the loop value, or rename the loop variable");
	auto it = defs_.find(name);
	if (it == defs_.end())
		return sym;
	const Def& d = it->second;
	if (d.function)
		throw Error("'" + name + "' is a function; call it as " + name + "(...)");
	if (d.formula)
		return resolve(d.body, defCtx(c, &it->first));
	if (!c.deep)
		return d.body; // a snapshot: its symbols are not looked up
	if (expanding_.count(name))
		return sym;
	expanding_.insert(name);
	struct Erase {
		std::set<std::string>& s;
		const std::string& n;
		~Erase() { s.erase(n); }
	} erase{expanding_, name};
	return resolve(d.body, defCtx(c, &it->first));
}

Expr Interpreter::resolveWith(const Expr& e, const Dyn& dyn, const Ctx& c, bool deep)
{
	Ctx g = c;
	g.dyn = &dyn;
	g.deep = deep;
	Memo fresh;
	g.memo = c.lex || c.inert ? &fresh : sharedMemo(&dyn, deep);
	return resolve(e, g);
}

// Resolves e with variable x treated as a free symbol.
Expr Interpreter::resolveFree(const Expr& e, const std::string& x, const Ctx& c)
{
	Dyn dyn{c.dyn, {{x, mkSym(x)}}, nextDyn_++};
	return resolveWith(e, dyn, c, c.deep);
}

long Interpreter::intArg(const Expr& call, size_t i, const Ctx& c, long lo, long hi)
{
	Expr v = resolve(call->args[i], c);
	long k;
	if (!isInt(v, k) || k < lo || k > hi)
		throw Error(call->name + ": argument " + std::to_string(i + 1) +
		            " must be an integer in [" + std::to_string(lo) + ", " +
		            std::to_string(hi) + "]");
	return k;
}

bool Interpreter::truthValue(const Expr& e, const Ctx& c)
{
	Expr v = resolve(e, c);
	if (v->kind != Kind::Num)
		throw Error("condition is not a number: " + toString(v));
	return !v->num.isZero();
}

Expr Interpreter::call(const Expr& e, const Ctx& c)
{
	const std::string& name = e->name;
	if (isOperator(name)) {
		if (name == "!")
			return mkInt(truthValue(e->args[0], c) ? 0 : 1);
		if (name == "&&")
			return mkInt(truthValue(e->args[0], c) && truthValue(e->args[1], c));
		if (name == "||")
			return mkInt(truthValue(e->args[0], c) || truthValue(e->args[1], c));
		Expr a = resolve(e->args[0], c), b = resolve(e->args[1], c);
		if (name == "==" || name == "!=") {
			bool eq = listsEqual(a, b);
			return mkInt(eq == (name == "=="));
		}
		if (a->kind != Kind::Num || b->kind != Kind::Num)
			throw Error("cannot compare " + toString(a) + " " + name + " " +
			            toString(b) + ": both sides must be numbers");
		int cmp = a->num.compare(b->num);
		bool r = name == "<" ? cmp < 0 : name == "<=" ? cmp <= 0
		       : name == ">" ? cmp > 0 : cmp >= 0;
		return mkInt(r);
	}
	if (name == "=")
		throw Error("an equation a = b can only be used in solve()");
	if (name == "%list") {
		Exprs items;
		for (const Expr& x : e->args)
			items.push_back(resolve(x, c));
		return mkList(items);
	}
	if (name == "%index") {
		Expr l = resolve(e->args[0], c), k = resolve(e->args[1], c);
		if (!isList(l))
			throw Error("only lists can be indexed: " + toString(l) + "[...]");
		long i;
		if (!isInt(k, i) || i < 1 || static_cast<size_t>(i) > l->args.size())
			throw Error("index " + toString(k) + " is not in 1.." + std::to_string(l->args.size()));
		return l->args[static_cast<size_t>(i - 1)];
	}
	if (findBuiltin(name))
		return callBuiltin(e, c);
	auto it = defs_.find(name);
	if (it != defs_.end() && it->second.function)
		return callUser(e, it->second, c);
	if (it != defs_.end())
		throw Error("'" + name + "' is a variable, not a function");
	throw Error("unknown function '" + name + "'");
}

Expr Interpreter::callUser(const Expr& e, const Def& d, const Ctx& c)
{
	const std::string& name = e->name;
	if (e->args.size() != d.params.size())
		throw Error(name + ": expected " + std::to_string(d.params.size()) +
		            " argument" + (d.params.size() == 1 ? "" : "s"));
	Bindings params;
	for (size_t i = 0; i < d.params.size(); i++) {
		if (e->args[i]->kind == Kind::Arg)
			throw Error(name + ": 'name=value' arguments are not allowed here");
		params[d.params[i]] = resolve(e->args[i], c);
	}
	DepthGuard depth(callDepth_);
	auto it = defs_.find(name);
	Memo fresh;
	Ctx g;
	g.lex = &params;
	g.dyn = c.dyn;
	g.deep = c.deep;
	g.inert = !d.formula; // the body of f(x) = ... is a snapshot
	g.defName = &it->first;
	g.memo = &fresh;
	return resolve(d.body, g);
}

// solve(eq[, x[, k]]): eq is a = b, x = b, or e (meaning e = 0); or a list
// of equations with a list of symbols, for a linear system
Expr Interpreter::callSolve(const Expr& e, const Ctx& c)
{
	const Expr& eqArg = e->args[0];
	// the symbols to solve for, as written (not evaluated)
	std::vector<std::string> xs;
	bool xList = false;
	if (e->args.size() >= 2) {
		const Expr& v = e->args[1];
		if (isList(v)) {
			xList = true;
			for (const Expr& a : v->args) {
				if (a->kind != Kind::Sym || isImUnit(a) || a->name == "pi")
					throw Error("solve: the symbols to solve for must be names, like [x, y]");
				xs.push_back(a->name);
			}
		} else {
			xs.push_back(varName(e, 1));
		}
	}
	Dyn freeDyn{c.dyn, {}, nextDyn_++};
	for (const std::string& x : xs)
		freeDyn.bindings[x] = mkSym(x);
	auto side = [&](const Expr& a) { return xs.empty() ? resolve(a, c) : resolveWith(a, freeDyn, c, c.deep); };
	auto toF = [&](const Expr& eq) {
		if (eq->kind == Kind::Arg) {
			if (isRange(eq))
				throw Error("solve: ranges (x = a to b) are only allowed in table(), seq() and sum()");
			return sub(side(mkSym(eq->name)), side(eq->args[0]));
		}
		if (eq->kind == Kind::Call && eq->name == "=")
			return sub(side(eq->args[0]), side(eq->args[1]));
		return side(eq);
	};
	Exprs fs;
	bool eqList = isList(eqArg);
	if (eqList) {
		for (const Expr& a : eqArg->args)
			fs.push_back(toF(a));
	} else {
		Expr f = toF(eqArg);
		if (isList(f)) { // a stored list of expressions
			eqList = true;
			fs = f->args;
		} else {
			fs.push_back(f);
		}
	}
	if (xs.empty()) { // the symbols in the equations
		std::set<std::string> vs;
		std::function<void(const Expr&)> walk = [&](const Expr& n) {
			if (n->kind == Kind::Sym && !isImUnit(n) && n->name != "pi")
				vs.insert(n->name);
			for (const Expr& a : n->args)
				walk(a);
		};
		for (const Expr& f : fs)
			walk(f);
		bool allNumbers = true;
		for (const Expr& f : fs)
			allNumbers = allNumbers && f->kind == Kind::Num;
		if (vs.empty() && allNumbers && fs.size() == 1 && e->args.size() == 1)
			return isZero(fs[0]) ? mkHeldOnly(mkDec("every value is a solution"),
			                                  "solve: every value is a solution; there is no list of them")
			                     : mkHeld(mkDec("no solution"), mkList({}));
		if (vs.size() != 1 && !eqList)
			throw Error("solve: say which symbol to solve for, like solve(e, x)");
		xs.assign(vs.begin(), vs.end());
	}
	if (eqList || xList || xs.size() > 1) {
		if (e->args.size() == 3)
			throw Error("solve: a system of equations has one solution; leave out k");
		if (fs.size() == 1 && xs.size() == 1 && !eqList) {
			// solve(e, [x]): as solve(e, x)
		} else {
			bool linear = true;
			LinearSolution ls;
			try {
				ls = solveLinear(fs, xs);
			} catch (const Error& err) {
				if (std::string(err.what()).find("not linear") == std::string::npos)
					throw;
				linear = false;
			}
			if (!linear) { // shown as x = 1, y = 2; x = -1, y = -2
				auto sets = solveSystem(fs, xs);
				if (sets.empty())
					return mkHeld(mkDec("no solution"), mkList({}));
				Exprs shown, values;
				for (const Exprs& v : sets) {
					Exprs items;
					for (size_t i = 0; i < xs.size(); i++)
						if (!(v[i]->kind == Kind::Sym && v[i]->name == xs[i]))
							items.push_back(mkArg(xs[i], v[i]));
					shown.push_back(mkCall("%set", items));
					values.push_back(mkList(v));
				}
				return mkHeld(mkCall("%sols", shown), mkList(values));
			}
			if (ls.none)
				return mkHeld(mkDec("no solution"), mkList({}));
			Exprs shown;
			for (size_t i = 0; i < xs.size(); i++)
				if (!ls.free[i])
					shown.push_back(mkArg(xs[i], ls.values[i]));
			if (shown.empty())
				return mkHeld(mkDec("every value is a solution"), mkList(ls.values));
			return mkHeld(mkCall("%sols", shown), mkList(ls.values));
		}
	}
	Expr f = fs[0];
	std::string x = xs[0];
	Solutions s = solve(f, x);
	if (hasAssumptions(x)) { // only the solutions that fit the assumptions
		Sign want = signOf(mkSym(x));
		bool wantNonZero = nonZero(mkSym(x));
		bool wantInt = isIntegerValued(mkSym(x));
		Exprs kept;
		for (const Expr& r : s.roots) {
			bool ok = true;
			Sign got = signOf(r);
			int sg = 0; // the sign of r, if known
			bool real = true, known = true;
			if (got == Sign::Pos || got == Sign::Neg || got == Sign::Zero) {
				sg = got == Sign::Pos ? 1 : got == Sign::Neg ? -1 : 0;
			} else {
				Expr v = numeric(r, 30);
				if (v->kind == Kind::Dec) {
					const std::string& t = v->name;
					real = t.find(imagName()) == std::string::npos;
					sg = t[0] == '-' ? -1 : t.find_first_not_of("0.") == std::string::npos ? 0 : 1;
				} else if (v->kind == Kind::Num) {
					sg = v->num.sign();
				} else {
					known = false; // depends on other symbols
				}
			}
			if (known) {
				if (want != Sign::Unknown && !real)
					ok = false;
				if (want == Sign::Pos) ok = ok && sg > 0;
				if (want == Sign::NonNeg) ok = ok && sg >= 0;
				if (want == Sign::Neg) ok = ok && sg < 0;
				if (want == Sign::NonPos) ok = ok && sg <= 0;
				if (wantNonZero)
					ok = ok && sg != 0;
				if (wantInt)
					ok = ok && r->kind == Kind::Num && r->num.isInt();
			}
			if (ok)
				kept.push_back(r);
		}
		s.roots = kept;
	}
	if (e->args.size() == 3) {
		long k = intArg(e, 2, c, 1, 1000000);
		if (s.all)
			throw Error("solve: every " + x + " is a solution");
		if (static_cast<size_t>(k) > s.roots.size())
			throw Error("solve: there " + std::string(s.roots.size() == 1 ? "is" : "are") +
			            " only " + std::to_string(s.roots.size()) + " solution" +
			            (s.roots.size() == 1 ? "" : "s"));
		return s.roots[static_cast<size_t>(k - 1)];
	}
	// shown as x = 1, x = 2; computes as the list [1, 2]
	if (s.all)
		return mkHeldOnly(mkDec("every " + x + " is a solution"),
		                  "solve: every " + x + " is a solution; there is no list of them");
	if (s.roots.empty() && s.unsolved.empty())
		return mkHeld(mkDec("no solution"), mkList({}));
	Exprs shown;
	for (const Expr& r : s.roots)
		shown.push_back(mkArg(x, r));
	for (const Expr& u : s.unsolved)
		shown.push_back(mkCall("=", {u, mkInt(0)}));
	if (!s.unsolved.empty())
		return mkHeldOnly(mkCall("%sols", shown),
		                  "solve: not all solutions were found, so there is no list of them; "
		                  "solve(e, x, k) gives solution number k");
	return mkHeld(mkCall("%sols", shown), mkList(s.roots));
}

Expr Interpreter::callBuiltin(const Expr& e, const Ctx& c)
{
	const std::string& name = e->name;
	const Builtin* b = findBuiltin(name);
	if (b->statementOnly)
		throw Error(name + "() can only be used as a statement on its own");
	size_t n = e->args.size();
	if (n < b->min || n > b->max) {
		if (b->min == b->max)
			throw Error(name + ": expected " + std::to_string(b->min) + " argument" +
			            (b->min == 1 ? "" : "s"));
		throw Error(name + ": expected " + std::to_string(b->min) + " to " +
		            std::to_string(b->max) + " arguments");
	}
	for (size_t i = 0; i < n; i++) {
		if (isRange(e->args[i]) && !((name == "seq" || name == "sum") && i == 1))
			throw Error(name + ": ranges (x = a to b) are only allowed in table(), seq() and sum()");
		if (e->args[i]->kind == Kind::Arg &&
		    !((name == "eval" || name == "subs") && i > 0) && !(name == "solve" && i == 0) &&
		    !((name == "seq" || name == "sum" || name == "taylor" || name == "limit") && i == 1))
			throw Error(name + ": 'name=value' arguments are not allowed here");
	}

	if (name == "eval" || name == "subs") {
		// the values are evaluated first, then applied together; both
		// also look into snapshots, plugging in current values
		Dyn dyn{c.dyn, {}, nextDyn_++};
		for (size_t i = 1; i < n; i++) {
			const Expr& a = e->args[i];
			if (a->kind != Kind::Arg)
				throw Error(name + ": arguments after the first must be name=value");
			checkNotReserved(a->name);
			dyn.bindings[a->name] = resolve(a->args[0], c);
		}
		if (name == "subs") // substitute first, then evaluate and simplify
			return resolveWith(e->args[0], dyn, c, true);
		// eval: evaluate and simplify first, with the named variables kept
		// as plain symbols, then insert the values
		Dyn freeDyn{c.dyn, {}, nextDyn_++};
		for (const auto& kv : dyn.bindings)
			freeDyn.bindings[kv.first] = mkSym(kv.first);
		Expr r = resolveWith(e->args[0], freeDyn, c, c.deep);
		if (hasVariableDenominator(r))
			r = cancel(r); // (x^2-1)/(x-1) -> x+1 before x is inserted
		return resolveWith(r, dyn, c, true);
	}
	static const std::pair<const char*, Expr (*)(const Expr&)> unary[] = {
		{"expand", expand}, {"cancel", cancel}, {"Re", realPart}, {"Im", imagPart},
		{"conj", conjugate}, {"abs", absValue}, {"trigexpand", trigexpand},
		{"trigreduce", trigreduce},
		{"sqrt", [](const Expr& x) { return simpPow(x, mkNum(Rational(1) / Rational(2))); }}};
	for (const auto& [fname, f] : unary)
		if (name == fname)
			return mapList(resolve(e->args[0], c), f);
	if (name == "solve")
		return callSolve(e, c);
	if (name == "factor") { // factor(e), factor(e, im): over Q(i)
		bool gauss = false;
		if (n == 2) {
			if (!isImUnit(e->args[1]))
				throw Error("factor: the second argument can only be " + imagName() +
				            ", to factor over the complex rationals");
			gauss = true;
		}
		return mapList(resolve(e->args[0], c), [gauss](const Expr& v) { return factor(v, gauss); });
	}
	if (name == "integrate") {
		const std::string& x = varName(e, 1);
		if (n == 3)
			throw Error("integrate: give both bounds, integrate(f, x, a, b)");
		Expr f = resolveFree(e->args[0], x, c);
		if (n == 2)
			return mapList(f, [&x](const Expr& v) { return integrate(v, x); });
		Expr a = resolve(e->args[2], c), b = resolve(e->args[3], c);
		return mapList(f, [&](const Expr& v) { return definiteIntegral(v, x, a, b); });
	}
	if (name == "taylor" || name == "limit") {
		// taylor(f, x = a, n), taylor(f, x, n) (at 0); limit(f, x = a[, left|right])
		const Expr& v = e->args[1];
		std::string x;
		Expr a = mkInt(0);
		if (v->kind == Kind::Arg && !isRange(v)) {
			x = v->name;
			checkNotReserved(x);
			a = resolve(v->args[0], c);
		} else if (v->kind == Kind::Sym && name == "taylor") {
			x = varName(e, 1);
		} else {
			throw Error(name + ": write the point as x = a, like " + name + "(f, x = 0" +
			            (name == "taylor" ? ", 5)" : ")"));
		}
		Expr f = resolveFree(e->args[0], x, c);
		if (name == "taylor") {
			long k = n > 2 ? intArg(e, 2, c, 0, 1000) : 5;
			return mapList(f, [&](const Expr& g) { return taylor(g, x, a, k); });
		}
		int dir = 0;
		if (n > 2) {
			const Expr& d = e->args[2];
			if (d->kind == Kind::Sym && (d->name == "left" || d->name == "right"))
				dir = d->name == "right" ? 1 : -1;
			else
				throw Error("limit: the direction is left or right");
		}
		return mapList(f, [&](const Expr& g) { return limit(g, x, a, dir); });
	}
	if (name == "length") {
		Expr l = resolve(e->args[0], c);
		if (!isList(l))
			throw Error("length: " + toString(l) + " is not a list");
		return mkInt(static_cast<long>(l->args.size()));
	}
	if (name == "seq" || name == "sum") {
		if (n == 1) { // sum(list)
			if (name == "seq")
				throw Error("seq: write seq(e, i = a to b)");
			Expr l = resolve(e->args[0], c);
			if (!isList(l))
				throw Error("sum: " + toString(l) + " is not a list; or write sum(e, i = a to b)");
			return broadcast(l->args, [](const Exprs& v) { return simpAdd(v); });
		}
		const Expr& r = e->args[1];
		if (!isRange(r))
			throw Error(name + ": the second argument must be a range, like i = 1 to 10");
		checkNotReserved(r->name);
		Rational v = numberOf(resolve(r->args[0], c), "the start of a range");
		Rational to = numberOf(resolve(r->args[1], c), "the end of a range");
		Rational by = numberOf(resolve(r->args[2], c), "the step of a range");
		if (by.isZero())
			throw Error(name + ": the step of a range cannot be 0");
		Exprs items;
		while (by.sign() > 0 ? v.compare(to) <= 0 : v.compare(to) >= 0) {
			if (items.size() >= 1000000)
				throw Error(name + ": more than 1000000 values");
			Dyn dyn{c.dyn, {{r->name, mkNum(v)}}, nextDyn_++};
			items.push_back(resolveWith(e->args[0], dyn, c, true));
			v = v + by;
		}
		if (name == "seq")
			return mkList(items);
		return items.empty() ? mkInt(0) : broadcast(items, [](const Exprs& a) { return simpAdd(a); });
	}
	if (isElementary(name) || name == "log") {
		Exprs a;
		for (const Expr& x : e->args)
			a.push_back(resolve(x, c));
		return broadcast(a, [&name](const Exprs& v) { return makeFunction(name, v); });
	}
	if (name == "gcd" || name == "lcm") {
		Exprs v;
		bool numbers = true;
		for (const Expr& a : e->args) {
			v.push_back(resolve(a, c));
			numbers = numbers && v.back()->kind == Kind::Num;
		}
		Expr r = v[0];
		if (numbers) {
			Rational acc = v[0]->num.abs();
			for (size_t i = 1; i < v.size(); i++)
				acc = name == "gcd" ? gcd(acc, v[i]->num) : lcm(acc, v[i]->num);
			return mkNum(acc);
		}
		if (v.size() == 1)
			return name == "gcd" ? polyGcd(r, mkInt(0)) : polyLcm(r, mkInt(1));
		for (size_t i = 1; i < v.size(); i++)
			r = name == "gcd" ? polyGcd(r, v[i]) : polyLcm(r, v[i]);
		return r;
	}
	if (name == "num") {
		long digits = n > 1 ? intArg(e, 1, c, 0, 1000000) : 20;
		return mapList(resolve(e->args[0], c),
		               [digits](const Expr& x) { return numeric(x, static_cast<size_t>(digits)); });
	}
	if (name == "diff") {
		const std::string& x = varName(e, 1);
		long k = n > 2 ? intArg(e, 2, c, 0, 100000) : 1;
		return mapList(resolveFree(e->args[0], x, c), [&x, k](const Expr& v) {
			Expr r = v;
			for (long i = 0; i < k; i++)
				r = diff(r, x);
			return r;
		});
	}
	if (name == "deg") {
		const std::string& x = varName(e, 1);
		return mapList(resolveFree(e->args[0], x, c),
		               [&x](const Expr& v) { return mkInt(polyDegree(v, x)); });
	}
	if (name == "coeff") {
		const std::string& x = varName(e, 1);
		long k = intArg(e, 2, c, 0, 1000000000L);
		return mapList(resolveFree(e->args[0], x, c),
		               [&x, k](const Expr& v) { return polyCoeff(v, x, k); });
	}
	if (name == "quo" || name == "rem") {
		const std::string& x = varName(e, 2);
		auto [q, r] = polyDivide(resolveFree(e->args[0], x, c),
		                         resolveFree(e->args[1], x, c), x);
		return name == "quo" ? q : r;
	}
	throw Error("unknown function '" + name + "'");
}

// ============================================================ definitions

bool Interpreter::depends(const Expr& e, const std::string& v) const
{
	Seen seen;
	return depends(e, v, seen);
}

bool Interpreter::depends(const Expr& e, const std::string& v, Seen& seen) const
{
	// a node seen before did not lead to v (we stop at the first hit)
	if (!seen.insert(e.get()).second)
		return false;
	switch (e->kind) {
	case Kind::Sym: {
		if (e->name == v)
			return true;
		auto w = defs_.find(e->name);
		return w != defs_.end() && w->second.formula && !w->second.function &&
		       depends(w->second.body, v, seen);
	}
	case Kind::Call: {
		for (size_t i = 0; i < e->args.size(); i++)
			if (!isVarPos(e, i) && !shadows(e, i, v) && depends(e->args[i], v, seen))
				return true;
		auto f = defs_.find(e->name);
		if (f != defs_.end() && f->second.function && f->second.formula) {
			for (const std::string& p : f->second.params)
				if (p == v)
					return false; // v is a parameter in the body
			return depends(f->second.body, v, seen);
		}
		return false;
	}
	default:
		for (const Expr& a : e->args)
			if (depends(a, v, seen))
				return true;
		return false;
	}
}

// Rewrites e so that it no longer depends on variable v, by substituting
// v's current definition (and inlining formulas that depend on v).
Expr Interpreter::inlineFor(const Expr& e, const std::string& v, Memo& memo) const
{
	auto m = memo.find(e.get());
	if (m != memo.end())
		return m->second;
	Expr r;
	switch (e->kind) {
	case Kind::Sym: {
		auto w = defs_.find(e->name);
		if (e->name == v) {
			if (w == defs_.end() || w->second.function)
				throw Error("recursive definition: '" + v + "' has no value yet");
			r = w->second.body;
		} else if (w != defs_.end() && w->second.formula && !w->second.function &&
		           depends(w->second.body, v)) {
			r = inlineFor(w->second.body, v, memo);
		} else {
			r = e;
		}
		break;
	}
	case Kind::Num:
	case Kind::Dec:
	case Kind::Str:
		r = e;
		break;
	default: {
		auto f = defs_.find(e->name);
		if (e->kind == Kind::Call && f != defs_.end() && f->second.function &&
		    f->second.formula && depends(f->second.body, v))
			throw Error("recursive definition: '" + v + "' depends on itself "
			            "through " + e->name + "()");
		auto n = std::make_shared<Node>(*e);
		for (size_t i = 0; i < n->args.size(); i++) {
			if (e->kind == Kind::Call && (isVarPos(e, i) || shadows(e, i, v)))
				continue;
			n->args[i] = inlineFor(e->args[i], v, memo);
		}
		r = n;
		break;
	}
	}
	memo.emplace(e.get(), r);
	return r;
}

// Replaces loop variables by their current values (a definition made
// inside a loop captures them). Parameters shadow loop variables.
Expr Interpreter::substituteLoopVars(const Expr& e, const std::vector<std::string>& params) const
{
	if (loopVars_.empty())
		return e;
	Bindings lv = loopBindings();
	for (const std::string& p : params)
		lv.erase(p);
	struct Sub {
		const Bindings& lv;
		Expr operator()(const Expr& x) const
		{
			if (x->kind == Kind::Sym) {
				auto it = lv.find(x->name);
				return it != lv.end() ? it->second : x;
			}
			if (x->args.empty())
				return x;
			auto n = std::make_shared<Node>(*x);
			for (size_t i = 0; i < n->args.size(); i++)
				if (!(x->kind == Kind::Call && isVarPos(x, i)))
					n->args[i] = (*this)(x->args[i]);
			return n;
		}
	};
	return Sub{lv}(e);
}

std::string Interpreter::defString(const std::string& name, const Def& d) const
{
	std::string s = name;
	if (d.function)
		s += "(" + joinParams(d.params) + ")";
	s += d.formula ? " := " : " = ";
	return s + toString(d.body);
}

// ============================================================ statements

Expr Interpreter::evaluate(const Expr& e)
{
	Bindings lex = loopBindings();
	Memo fresh;
	Ctx c;
	c.lex = lex.empty() ? nullptr : &lex;
	c.memo = lex.empty() ? sharedMemo(nullptr, false) : &fresh;
	return resolveKeep(e, c);
}

bool Interpreter::truth(const Expr& cond)
{
	shared_.clear(); // definitions may have changed since the last evaluation
	Expr v = heldValue(evaluate(cond));
	if (v->kind != Kind::Num)
		throw Error("condition is not a number: " + toString(v));
	return !v->num.isZero();
}

void Interpreter::execAssign(const Stmt& s)
{
	checkNotReserved(s.name);
	for (const std::string& p : s.params)
		checkNotReserved(p);
	checkStatementOnly(s.expr, false);
	if (isLoopVar(s.name))
		throw Error("cannot assign to the loop variable '" + s.name + "'");
	Def d;
	d.function = s.isFunction;
	d.params = s.params;
	if (s.isFunction) {
		if (findBuiltin(s.name))
			throw Error("cannot redefine the built-in function '" + s.name + "'");
		// evaluate the body now, with the parameters as plain symbols
		Bindings lex = loopBindings();
		for (const std::string& p : s.params)
			lex[p] = mkSym(p);
		Memo fresh;
		Ctx c;
		c.lex = &lex;
		c.memo = &fresh;
		d.body = resolve(s.expr, c);
	} else {
		if (depends(s.expr, s.name)) {
			auto old = defs_.find(s.name);
			if (old == defs_.end() || old->second.function)
				throw Error("'" + s.name + "' has no value yet, so " + s.name +
				            " = ... cannot use it");
		}
		d.body = evaluate(s.expr);
	}
	defs_[s.name] = d;
	if (s.show)
		emitDef(s.name, d);
}

void Interpreter::execDefine(const Stmt& s)
{
	checkNotReserved(s.name);
	for (const std::string& p : s.params)
		checkNotReserved(p);
	checkStatementOnly(s.expr, false);
	if (isLoopVar(s.name))
		throw Error("cannot assign to the loop variable '" + s.name + "'");
	if (s.isFunction && findBuiltin(s.name))
		throw Error("cannot redefine the built-in function '" + s.name + "'");
	Expr raw = substituteLoopVars(s.expr, s.params);
	if (!s.isFunction && depends(raw, s.name)) {
		Memo memo;
		raw = inlineFor(raw, s.name, memo);
	}
	Def d;
	d.formula = true;
	d.function = s.isFunction;
	d.params = s.params;
	d.body = safeSimplify(raw);
	defs_[s.name] = d;
	if (s.show)
		emitDef(s.name, d);
}

void Interpreter::execFor(const Stmt& s)
{
	checkNotReserved(s.name);
	Expr start = evaluate(s.expr), stop = evaluate(s.stop), step = evaluate(s.step);
	Rational v = numberOf(start, "the start of a for loop");
	Rational to = numberOf(stop, "the end of a for loop");
	Rational by = numberOf(step, "the step of a for loop");
	if (by.isZero())
		throw Error("the step of a for loop cannot be 0");
	while (by.sign() > 0 ? v.compare(to) <= 0 : v.compare(to) >= 0) {
		loopVars_.emplace_back(s.name, mkNum(v));
		{
			LoopGuard guard(loopVars_);
			execBlock(s.blocks[0]);
		}
		if (quit_)
			return;
		v = v + by;
	}
}

void Interpreter::execCFor(const Stmt& s)
{
	checkNotReserved(s.name);
	// for (i = init; cond; i = step): i is a loop variable like in the
	// range form, visible in cond, step and body only
	Expr v = evaluate(s.expr);
	for (;;) {
		loopVars_.emplace_back(s.name, v);
		LoopGuard guard(loopVars_);
		curLine_ = s.line;
		if (!truth(s.stop))
			return;
		execBlock(s.blocks[0]);
		if (quit_)
			return;
		curLine_ = s.line;
		shared_.clear();
		v = evaluate(s.step);
	}
}

void Interpreter::table(const Expr& e)
{
	Exprs columns, ranges;
	for (const Expr& a : e->args) {
		if (isRange(a)) {
			checkNotReserved(a->name);
			ranges.push_back(a);
		}
		else if (a->kind == Kind::Arg)
			throw Error("table: write a range as " + a->name + " = start to stop");
		else
			columns.push_back(a);
	}
	if (columns.empty())
		throw Error("table: nothing to show");
	Bindings lex = loopBindings();
	Ctx base;
	base.lex = lex.empty() ? nullptr : &lex;
	Memo baseMemo;
	base.memo = &baseMemo;

	// iterate the ranges, the first one outermost
	struct Rec {
		Interpreter& self;
		const Exprs& columns;
		const Exprs& ranges;
		void run(size_t k, const Ctx& c)
		{
			if (k == ranges.size()) {
				Memo fresh;
				Ctx g = c;
				g.deep = true; // like eval: plug values into snapshots
				g.memo = &fresh;
				Exprs row;
				for (const Expr& col : columns)
					row.push_back(self.resolve(col, g));
				self.emitRow(row);
				return;
			}
			const Expr& r = ranges[k];
			Rational v = numberOf(self.resolve(r->args[0], c), "the start of a range");
			Rational to = numberOf(self.resolve(r->args[1], c), "the end of a range");
			Rational by = numberOf(self.resolve(r->args[2], c), "the step of a range");
			if (by.isZero())
				throw Error("table: the step of a range cannot be 0");
			while (by.sign() > 0 ? v.compare(to) <= 0 : v.compare(to) >= 0) {
				Dyn dyn{c.dyn, {{r->name, mkNum(v)}}, self.nextDyn_++};
				Memo fresh;
				Ctx g = c;
				g.dyn = &dyn;
				g.memo = &fresh;
				run(k + 1, g);
				v = v + by;
			}
		}
	};
	Rec{*this, columns, ranges}.run(0, base);
}

bool Interpreter::execStatementCall(const Stmt& s)
{
	const Expr& e = s.expr;
	if (e->kind != Kind::Call)
		return false;
	const Builtin* b = findBuiltin(e->name);
	if (!b || !b->statementOnly)
		return false;
	size_t n = e->args.size();
	if (n < b->min || n > b->max)
		throw Error(e->name + ": expected " + std::to_string(b->min) + " argument" +
		            (b->min == 1 ? "" : "s"));
	checkStatementOnly(e, true);
	if (e->name == "unset") {
		for (size_t i = 0; i < n; i++)
			checkNotReserved(varName(e, i));
		for (const Expr& a : e->args)
			defs_.erase(a->name);
	} else if (e->name == "def") {
		const std::string& x = varName(e, 0);
		auto v = defs_.find(x);
		if (v == defs_.end())
			throw Error("def: '" + x + "' is not defined");
		emitDef(x, v->second);
	} else if (e->name == "save" || e->name == "transcript") {
		if (e->args[0]->kind != Kind::Str)
			throw Error(e->name + ": the file name must be a string, like " +
			            e->name + "(\"work.xyz\")");
		saveSession(e->args[0]->name, e->name == "transcript", s.show);
	} else if (e->name == "disp") {
		Exprs row;
		for (size_t i = 0; i < n; i++) {
			if (e->args[i]->kind == Kind::Arg)
				throw Error("disp: 'name=value' arguments are not allowed here");
			row.push_back(evaluate(e->args[i]));
		}
		emitRow(row);
	} else if (e->name == "assume") {
		// assume(x > 0, y <= 0, ...), assume(n, integer)
		if (n == 2 && e->args[0]->kind == Kind::Sym && e->args[1]->kind == Kind::Sym &&
		    e->args[1]->name == "integer") {
			checkNotReserved(e->args[0]->name);
			setAssumption(e->args[0]->name, Sign::Unknown, true);
		} else {
			for (const Expr& a : e->args) {
				const char* usage = "assume: write assume(x > 0), assume(x >= 0), "
				                    "assume(x < 0), assume(x <= 0), assume(x != 0) or "
				                    "assume(n, integer)";
				if (a->kind != Kind::Call || a->args.size() != 2)
					throw Error(usage);
				Expr l = a->args[0], r = a->args[1];
				std::string op = a->name;
				if (isZero(l) && r->kind == Kind::Sym) { // 0 < x
					std::swap(l, r);
					op = op == "<" ? ">" : op == ">" ? "<" : op == "<=" ? ">=" : op == ">=" ? "<=" : op;
				}
				if (l->kind != Kind::Sym || !isZero(r) || isImUnit(l) || l->name == "pi" ||
				    l->name == "inf")
					throw Error(usage);
				Sign s = op == ">" ? Sign::Pos : op == ">=" ? Sign::NonNeg : op == "<" ? Sign::Neg
				       : op == "<=" ? Sign::NonPos : op == "!=" ? Sign::NonZero : Sign::Unknown;
				if (s == Sign::Unknown)
					throw Error(usage);
				setAssumption(l->name, s, false);
			}
		}
	} else if (e->name == "forget") {
		for (size_t i = 0; i < n; i++)
			forgetAssumption(varName(e, i));
	} else if (e->name == "assumptions") {
		auto all = listAssumptions();
		if (all.empty())
			emitText("no assumptions");
		for (const std::string& t : all)
			emitText(t);
	} else if (e->name == "latex") {
		if (e->args[0]->kind == Kind::Arg)
			throw Error("latex: 'name=value' arguments are not allowed here");
		emitLatex(evaluate(e->args[0]));
	} else {
		table(e);
	}
	return true;
}

void Interpreter::saveSession(const std::string& file, bool withOutput, bool show)
{
	std::ofstream f(file);
	if (!f)
		throw Error("cannot write the file '" + file + "'");
	bool tex = file.size() > 4 && file.compare(file.size() - 4, 4, ".tex") == 0;
	if (tex) {
		writeLatex(f, withOutput);
	} else {
	if (imagName() != "im")
		f << "# the imaginary unit is " << imagName() << ": run with  xyz --imag="
		  << imagName() << " " << file << "\n";
	for (const LogEntry& e : log_) {
		f << e.source << (e.show ? "\n" : ";\n");
		if (!withOutput || e.output.empty())
			continue;
		// output as comments: the file can still be run with xyz
		size_t b = 0;
		while (b < e.output.size()) {
			size_t n = e.output.find('\n', b);
			if (n == std::string::npos)
				n = e.output.size();
			f << "# " << e.output.substr(b, n - b) << '\n';
			b = n + 1;
		}
	}
	}
	f.close();
	if (!f)
		throw Error("cannot write the file '" + file + "'");
	if (show)
		emitText("saved " + std::to_string(log_.size()) + " statement" +
		         (log_.size() == 1 ? "" : "s") + " to " + file);
}

// ---------------------------------------------------------------- output

void Interpreter::emitValue(const Expr& v)
{
	shown_.push_back(v);
	out_ << toString(v) << '\n';
	if (items_)
		items_->push_back({OutItem::Type::Value, {v}, {}, {}, {}});
}

void Interpreter::emitLatex(const Expr& v)
{
	out_ << toLatex(v) << '\n';
	if (items_)
		items_->push_back({OutItem::Type::Value, {v}, {}, {}, {}});
}

void Interpreter::emitDef(const std::string& name, const Def& d)
{
	shown_.push_back(d.body);
	out_ << defString(name, d) << '\n';
	if (items_)
		items_->push_back({OutItem::Type::Def, {}, name, d, {}});
}

void Interpreter::emitRow(const Exprs& v)
{
	shown_.insert(shown_.end(), v.begin(), v.end());
	std::string line;
	for (size_t i = 0; i < v.size(); i++)
		line += (i ? "\t" : "") + toString(v[i]);
	out_ << line << '\n';
	if (items_)
		items_->push_back({OutItem::Type::Row, v, {}, {}, {}});
}

void Interpreter::emitText(const std::string& t)
{
	out_ << t << '\n';
	if (items_)
		items_->push_back({OutItem::Type::Text, {}, {}, {}, t});
}

std::string Interpreter::defLatex(const std::string& name, const Def& d) const
{
	std::string s = latexName(name);
	if (d.function) {
		s += "\\left(";
		for (size_t i = 0; i < d.params.size(); i++)
			s += (i ? ", " : "") + latexName(d.params[i]);
		s += "\\right)";
	}
	return s + (d.formula ? " \\mathrel{:=} " : " = ") + toLatex(d.body);
}

// A LaTeX document: every statement verbatim, followed by its results as
// formulas (tables as arrays, errors in italics).
void Interpreter::writeLatex(std::ostream& f, bool withOutput) const
{
	f << "% written by xyz transcript()\n"
	     "\\documentclass{article}\n"
	     "\\usepackage{amsmath}\n"
	     "\\begin{document}\n";
	if (imagName() != "im")
		f << "% the imaginary unit was called " << imagName() << " (xyz --imag="
		  << imagName() << ")\n";
	for (const LogEntry& e : log_) {
		f << "\n\\begin{verbatim}\n" << e.source << (e.show ? "" : ";")
		  << "\n\\end{verbatim}\n";
		if (!withOutput)
			continue;
		const auto& it = e.items;
		for (size_t i = 0; i < it.size();) {
			using T = OutItem::Type;
			auto single = [](const OutItem& o) {
				return o.type == T::Value || o.type == T::Def ||
				       (o.type == T::Row && o.values.size() == 1);
			};
			if (single(it[i])) { // consecutive results, one per line
				f << "\\begin{gather*}\n";
				for (bool first = true; i < it.size() && single(it[i]); i++, first = false) {
					f << (first ? "" : " \\\\\n")
					  << (it[i].type == T::Def ? defLatex(it[i].name, it[i].def)
					                           : toLatex(it[i].values[0]));
				}
				f << "\n\\end{gather*}\n";
			} else if (it[i].type == T::Row) { // consecutive rows: an array
				size_t cols = 0, j = i;
				for (; j < it.size() && it[j].type == T::Row && it[j].values.size() > 1; j++)
					cols = std::max(cols, it[j].values.size());
				f << "\\[\n\\begin{array}{" << std::string(cols, 'c') << "}\n";
				for (; i < j; i++) {
					for (size_t k = 0; k < it[i].values.size(); k++)
						f << (k ? " & " : "") << toLatex(it[i].values[k]);
					f << (i + 1 < j ? " \\\\\n" : "\n");
				}
				f << "\\end{array}\n\\]\n";
			} else {
				f << (it[i].type == T::Error ? "\\textit{" : "\\texttt{")
				  << latexText(it[i].text) << "}\\par\n";
				i++;
			}
		}
	}
	f << "\n\\end{document}\n";
}

void Interpreter::execBlock(const std::vector<Stmt>& body)
{
	for (const Stmt& s : body) {
		exec(s);
		if (quit_)
			return;
	}
}

void Interpreter::exec(const Stmt& s)
{
	curLine_ = s.line;
	shared_.clear(); // definitions may have changed
	expanding_.clear();
	switch (s.type) {
	case Stmt::Type::Empty:
		return;
	case Stmt::Type::Quit:
		quit_ = true;
		return;
	case Stmt::Type::Assign:
		execAssign(s);
		return;
	case Stmt::Type::Define:
		execDefine(s);
		return;
	case Stmt::Type::Expr: {
		if (execStatementCall(s))
			return;
		checkStatementOnly(s.expr, true);
		Expr v = evaluate(s.expr);
		if (s.show)
			emitValue(v);
		return;
	}
	case Stmt::Type::If:
		execBranches(s);
		return;
	case Stmt::Type::Else:
		throw Error("'else' without a preceding 'if'");
	case Stmt::Type::While:
		for (;;) {
			curLine_ = s.line;
			if (!truth(s.expr))
				return;
			execBlock(s.blocks[0]);
			if (quit_)
				return;
		}
	case Stmt::Type::DoWhile:
		do {
			execBlock(s.blocks[0]);
			if (quit_)
				return;
			curLine_ = s.line;
		} while (truth(s.expr));
		return;
	case Stmt::Type::For:
		if (s.cStyle)
			execCFor(s);
		else
			execFor(s);
		return;
	}
}

bool Interpreter::execBranches(const Stmt& s)
{
	for (size_t i = 0; i < s.conds.size(); i++) {
		curLine_ = s.line;
		if (truth(s.conds[i])) {
			execBlock(s.blocks[i]);
			return true;
		}
	}
	if (s.hasElse) {
		execBlock(s.blocks.back());
		return true;
	}
	return false;
}

void Interpreter::report(int line, const std::string& msg)
{
	out_.flush();
	std::string text = "error: ";
	if (showLines_)
		text += "line " + std::to_string(line) + ": ";
	text += msg;
	err_ << text << '\n';
	err_.flush();
	if (items_)
		items_->push_back({OutItem::Type::Error, {}, {}, {}, text});
}

int Interpreter::run(const std::string& text)
{
	int errors = 0;
	Parser p(text);
	for (;;) {
		std::optional<Stmt> s;
		try {
			s = p.next();
		} catch (const SyntaxError& e) {
			report(e.line, e.what());
			errors++;
			p.recover();
			continue;
		}
		if (!s)
			break;
		IfState next = IfState::None;
		curLine_ = s->line;
		// record the statement and its output for save() / transcript(),
		// except save and transcript themselves
		bool record = s->type != Stmt::Type::Empty && !s->source.empty() &&
		              !(s->type == Stmt::Type::Expr && s->expr->kind == Kind::Call &&
		                (s->expr->name == "save" || s->expr->name == "transcript"));
		LogEntry entry{s->source, s->show, {}, {}};
		if (record) {
			outBuf_.sink = errBuf_.sink = &entry.output;
			items_ = &entry.items;
		}
		shown_.clear();
		try {
			if (s->type == Stmt::Type::Else) {
				// continues the if executed just before (typed on a later line)
				if (lastIf_ == IfState::None)
					throw Error("'else' without a preceding 'if'");
				next = IfState::Taken;
				if (lastIf_ == IfState::NotTaken && !execBranches(*s))
					next = s->hasElse ? IfState::None : IfState::NotTaken;
				if (s->hasElse)
					next = IfState::None; // a plain else ends the chain
			} else if (s->type == Stmt::Type::If) {
				bool taken = execBranches(*s);
				next = s->hasElse ? IfState::None
				       : taken    ? IfState::Taken : IfState::NotTaken;
			} else {
				exec(*s);
			}
		} catch (const Error& e) {
			report(curLine_, e.what());
			errors++;
			next = IfState::None;
		} catch (const std::bad_alloc&) {
			report(curLine_, "out of memory");
			errors++;
			next = IfState::None;
		}
		out_.flush();
		err_.flush();
		outBuf_.sink = errBuf_.sink = nullptr;
		items_ = nullptr;
		if (interactive_ && !hinted_) { // once: what an assumption would do
			for (const Expr& v : shown_) {
				std::string h = assumptionHint(v);
				if (!h.empty()) {
					err_ << "note: " << h << " (this note is shown once)\n";
					err_.flush();
					hinted_ = true;
					break;
				}
			}
		}
		if (record)
			log_.push_back(std::move(entry));
		if (s->type != Stmt::Type::Empty)
			lastIf_ = next;
		loopVars_.clear();
		callDepth_ = 0;
		shared_.clear();
		if (quit_)
			break;
	}
	BclSession::gc();
	out_.flush();
	return errors;
}

} // namespace xyz
