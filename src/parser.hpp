// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lexer and recursive-descent parser.
//
//   program   := { stmt sep }                     sep: ';' (hides output),
//                                                 newline, '}' or end
//   stmt      := empty | 'quit' | 'halt'
//              | target ('=' | ':=') expr        assignment / definition
//              | 'if' '(' expr ')' block { 'else' 'if' '(' expr ')' block }
//                                    [ 'else' block ]
//              | 'while' '(' expr ')' block
//              | 'do' block 'while' '(' expr ')'
//              | 'for' '(' NAME '=' expr 'to' expr ['step' expr] ')' block
//              | 'for' '(' NAME '=' expr ';' expr ';' NAME '=' expr ')' block
//              | 'else' ...      continues the if statement just executed
//                                (when the else comes in a later chunk)
//              | expr
//   target    := NAME | NAME '(' [NAME {',' NAME}] ')'
//   block     := '{' { stmt sep } '}'
//   expr      := and { '||' and }
//   and       := not { '&&' not }
//   not       := '!' not | rel
//   rel       := sum [ ('=='|'!='|'<'|'<='|'>'|'>=') sum ]
//   sum       := term { ('+'|'-') term }
//   term      := unary { ('*'|'/') unary }
//   unary     := ('-'|'+') unary | power
//   power     := primary [ '^' unary ]           (right associative)
//   primary   := NUMBER | NAME | NAME '(' [args] ')' | '(' expr ')'
//   args      := arg { ',' arg }
//   arg       := NAME '=' expr ['to' expr ['step' expr]] | expr
//
// A number directly followed by a name is a product: 2x = 2*x.
// Comments: '#' to end of line and /* ... */. Newlines inside parentheses
// and backslash-newline do not end a statement.
#pragma once

#include "expr.hpp"

#include <optional>
#include <string>
#include <vector>

namespace xyz {

struct Stmt {
	enum class Type {
		Empty, Expr, Assign, Define, Quit, If, Else, While, DoWhile, For
	} type = Type::Empty;
	bool cStyle = false;             // For: C-style for (init; cond; step)
	bool hasElse = false;            // If/Else: ends with a plain else
	bool show = true; // false when the statement ends with ';'
	int line = 0;
	std::string name;                // Assign/Define target, For variable
	bool isFunction = false;         // Assign/Define: name(params) = ...
	std::vector<std::string> params; // function parameters
	Expr expr;                       // value / condition / for start
	Expr stop, step;                 // For: stop (C-style: condition), step
	Exprs conds;                     // If/Else: one condition per branch
	std::vector<std::vector<Stmt>> blocks; // If: branches (+ else); loops: body
};

struct SyntaxError : Error {
	int line;
	bool atEof; // the input ended too early (more lines may complete it)
	SyntaxError(const std::string& msg, int l, bool eof = false)
		: Error(msg), line(l), atEof(eof) {}
};

// Words that cannot be used as names.
bool isKeyword(const std::string& s);

class Parser {
public:
	explicit Parser(const std::string& text);
	// Next top-level statement, or nullopt at the end. Throws SyntaxError;
	// call recover() afterwards to skip the rest of the bad statement.
	std::optional<Stmt> next();
	void recover();

private:
	enum class T { Eof, Sep, Num, Id, Kw, Op, Bad };
	struct Tok {
		T type = T::Eof;
		std::string text; // Num/Id/Kw text, Op spelling, Sep ";" or "\n"
		int line = 1;
	};

	void tokenize(const std::string& src);
	const Tok& tok() const { return toks_[pos_]; }
	const Tok& at(size_t k) const;
	void advance();
	bool isOp(const char* op) const { return tok().type == T::Op && tok().text == op; }
	bool isKw(const char* kw) const { return tok().type == T::Kw && tok().text == kw; }
	void expectOp(const char* op, const char* what);
	[[noreturn]] void syntax(const std::string& what) const;

	Stmt statement();
	std::vector<Stmt> block();
	void elseChain(Stmt& s); // parses  [else if (c) {..}]... [else {..}]
	void skipNewlines();
	void endStatement(Stmt& s); // consumes the separator, sets show
	Expr parenCondition();

	Expr expr();
	Expr andExpr();
	Expr notExpr();
	Expr relExpr();
	Expr sum();
	Expr term();
	Expr unary();
	Expr power();
	Expr primary();
	Expr argument();

	std::vector<Tok> toks_;
	size_t pos_ = 0;
	size_t stmtStart_ = 0;
};

// True if text is an unfinished statement (open parentheses or braces,
// 'if (c)' without its block, a trailing backslash, ...): used to read
// more lines in interactive mode.
bool textIncomplete(const std::string& text);

} // namespace xyz
