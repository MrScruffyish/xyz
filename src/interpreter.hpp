// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Variables, functions, evaluation, built-ins and statement execution.
//
// Assignment
// ----------
//   a = expr       evaluates expr now and stores the result (a snapshot).
//                  It never changes until a is assigned again. Free
//                  symbols inside it stay plain symbols; eval() plugs in
//                  the current values on request.
//   a := expr      stores expr itself (a formula); every use re-evaluates
//                  it with the values at that time.
//   f(x) = expr    function whose body is evaluated once, now.
//   f(x) := expr   function whose body is stored and evaluated at each call.
//
// Scopes
// ------
// Loop variables and function parameters are lexical: they are visible
// only in the text of the loop body / function body. eval(e, x=v) and
// table(...) bind dynamically: the binding applies everywhere while e is
// evaluated, including inside stored formulas and snapshots.
#pragma once

#include "expr.hpp"
#include "parser.hpp"

#include <map>
#include <ostream>
#include <streambuf>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xyz {

// Writes to another stream buffer and, while sink is set, also appends
// everything to *sink (used to record the output of each statement).
class TeeBuf : public std::streambuf {
public:
	explicit TeeBuf(std::streambuf* real) : real_(real) {}
	std::string* sink = nullptr;

protected:
	int overflow(int c) override
	{
		if (c == traits_type::eof())
			return 0;
		if (sink)
			sink->push_back(static_cast<char>(c));
		return real_->sputc(static_cast<char>(c));
	}
	std::streamsize xsputn(const char* s, std::streamsize n) override
	{
		if (sink)
			sink->append(s, static_cast<size_t>(n));
		return real_->sputn(s, n);
	}
	int sync() override { return real_->pubsync(); }

private:
	std::streambuf* real_;
};

class Interpreter {
public:
	Interpreter(std::ostream& out, std::ostream& err)
		: outBuf_(out.rdbuf()), errBuf_(err.rdbuf()), out_(&outBuf_), err_(&errBuf_)
	{
	}

	// Runs program text; returns the number of errors. Sets quit() when a
	// quit statement is executed.
	int run(const std::string& text);
	bool quit() const { return quit_; }
	void setShowLines(bool b) { showLines_ = b; } // "error: line N: ..."

private:
	struct Def {
		bool formula = false;  // stored with :=
		bool function = false;
		std::vector<std::string> params;
		Expr body;
	};
	using Bindings = std::map<std::string, Expr>;
	using Memo = std::unordered_map<const Node*, Expr>;
	struct Dyn {
		const Dyn* up;
		Bindings bindings;
		unsigned long id;
	};
	struct Ctx {
		const Bindings* lex = nullptr; // loop variables / parameters
		const Dyn* dyn = nullptr;      // eval() / table() bindings
		bool deep = false;             // look into snapshots (eval)
		bool inert = false;            // free symbols are not looked up
		const std::string* defName = nullptr; // inside this stored definition
		Memo* memo = nullptr;
	};

	// statements
	void exec(const Stmt& s);
	void execBlock(const std::vector<Stmt>& body);
	void execAssign(const Stmt& s);
	void execDefine(const Stmt& s);
	void execFor(const Stmt& s);
	void execCFor(const Stmt& s);
	bool execBranches(const Stmt& s); // if/else chain; true if a branch ran
	bool execStatementCall(const Stmt& s); // disp, def, unset, table, save
	void saveSession(const std::string& file, bool withOutput, bool show);
	void table(const Expr& call);
	bool truth(const Expr& cond);
	Expr evaluate(const Expr& e); // in the context of the current statement
	void report(int line, const std::string& msg);
	std::string defString(const std::string& name, const Def& d) const;

	// evaluation
	Expr resolve(const Expr& e, const Ctx& c);     // the value (Held: its value)
	Expr resolveKeep(const Expr& e, const Ctx& c); // Held results kept, to be shown
	Expr lookup(const Expr& sym, const Ctx& c);
	Expr call(const Expr& e, const Ctx& c);
	Expr callBuiltin(const Expr& e, const Ctx& c);
	Expr callSolve(const Expr& e, const Ctx& c);
	Expr callUser(const Expr& e, const Def& d, const Ctx& c);
	Expr resolveFree(const Expr& e, const std::string& x, const Ctx& c);
	Expr resolveWith(const Expr& e, const Dyn& dyn, const Ctx& c, bool deep);
	bool truthValue(const Expr& e, const Ctx& c);
	long intArg(const Expr& call, size_t i, const Ctx& c, long lo, long hi);
	Ctx defCtx(const Ctx& c, const std::string* name);
	Memo* sharedMemo(const Dyn* dyn, bool deep);
	Bindings loopBindings() const;
	bool isLoopVar(const std::string& name) const;

	// definitions
	using Seen = std::unordered_set<const Node*>;
	bool depends(const Expr& e, const std::string& v) const;
	bool depends(const Expr& e, const std::string& v, Seen& seen) const;
	Expr inlineFor(const Expr& e, const std::string& v, Memo& memo) const;
	Expr substituteLoopVars(const Expr& e, const std::vector<std::string>& params) const;

	std::map<std::string, Def> defs_;
	std::vector<std::pair<std::string, Expr>> loopVars_; // innermost last
	TeeBuf outBuf_, errBuf_;
	std::ostream out_;
	std::ostream err_;

	// what a statement printed, kept as values for LaTeX transcripts
	struct OutItem {
		enum class Type { Value, Def, Row, Text, Error } type;
		Exprs values;     // Value: one, Row: the columns
		std::string name; // Def
		Def def;          // Def
		std::string text; // Text, Error
	};
	// everything run so far, for save() and transcript()
	struct LogEntry {
		std::string source;         // the statement as typed
		bool show;                  // false if it ended with ';'
		std::string output;         // what it printed (results and errors)
		std::vector<OutItem> items; // the same, as values
	};
	std::vector<LogEntry> log_;
	std::vector<OutItem>* items_ = nullptr; // recording for the current statement

	// all results are printed through these
	void emitValue(const Expr& v);
	void emitLatex(const Expr& v);
	void emitDef(const std::string& name, const Def& d);
	void emitRow(const Exprs& v);
	void emitText(const std::string& t);
	void writeLatex(std::ostream& f, bool withOutput) const;
	std::string defLatex(const std::string& name, const Def& d) const;
	bool quit_ = false;
	bool showLines_ = true;
	int curLine_ = 0;
	// state of the last top-level if, so that an else typed on a later
	// line can continue it
	enum class IfState { None, Taken, NotTaken } lastIf_ = IfState::None;

	// per-statement evaluation state
	std::map<std::pair<unsigned long, bool>, Memo> shared_;
	std::set<std::string> expanding_; // snapshot expansion in eval()
	unsigned long nextDyn_ = 1;
	int callDepth_ = 0;
};

} // namespace xyz
