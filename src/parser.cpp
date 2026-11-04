// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
#include "parser.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace xyz {

namespace {

bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
bool isIdStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

const char* const kKeywords[] = {"if", "else", "while", "do", "for",
                                 "to", "step", "quit", "halt"};

// two-character operators first
const char* const kOps[] = {":=", "==", "!=", "<=", ">=", "&&", "||",
                            "+", "-", "*", "/", "^", "(", ")", ",", "=",
                            "<", ">", "!", "{", "}"};

} // namespace

bool isKeyword(const std::string& s)
{
	for (const char* k : kKeywords)
		if (s == k)
			return true;
	return false;
}

Parser::Parser(const std::string& text) { tokenize(text); }

void Parser::tokenize(const std::string& s)
{
	src_ = s;
	size_t i = 0;
	int line = 1, depth = 0;
	auto at = [&s](size_t k) { return k < s.size() ? s[k] : '\0'; };
	// a token covering s[b, i)
	auto push = [this, &i](T t, std::string text, int l, size_t b) {
		toks_.push_back(Tok{t, std::move(text), l, b, i});
	};
	while (i < s.size()) {
		char c = s[i];
		size_t b = i;
		if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
			i++;
		} else if (c == '\\' && at(i + 1) == '\n') {
			i += 2;
			line++;
		} else if (c == '\\' && at(i + 1) == '\r' && at(i + 2) == '\n') {
			i += 3;
			line++;
		} else if (c == '#') {
			while (i < s.size() && s[i] != '\n')
				i++;
		} else if (c == '/' && at(i + 1) == '*') {
			size_t end = s.find("*/", i + 2);
			if (end == std::string::npos) {
				push(T::Bad, "unterminated comment", line, b);
				break;
			}
			for (size_t k = i; k < end; k++)
				if (s[k] == '\n')
					line++;
			i = end + 2;
		} else if (c == '\n') {
			i++;
			if (depth == 0)
				push(T::Sep, "\n", line, b);
			line++;
		} else if (c == ';') {
			i++;
			push(T::Sep, ";", line, b);
		} else if (c == '"') {
			size_t end = s.find_first_of("\"\n", i + 1);
			if (end == std::string::npos || s[end] != '"') {
				i = end == std::string::npos ? s.size() : end;
				push(T::Bad, "unterminated string", line, b);
				depth = 0; // the statement ends with this line
				continue;
			}
			i = end + 1;
			push(T::Str, s.substr(b + 1, end - b - 1), line, b);
		} else if (isDigit(c) || (c == '.' && isDigit(at(i + 1)))) {
			while (isDigit(at(i)))
				i++;
			if (at(i) == '.') {
				i++;
				while (isDigit(at(i)))
					i++;
			}
			push(T::Num, s.substr(b, i - b), line, b);
			if (isIdStart(at(i))) // 2x means 2*x
				push(T::Op, "*", line, i);
		} else if (isIdStart(c)) {
			while (isIdChar(at(i)))
				i++;
			std::string w = s.substr(b, i - b);
			push(isKeyword(w) ? T::Kw : T::Id, w, line, b);
		} else {
			const char* op = nullptr;
			for (const char* o : kOps)
				if (s.compare(i, std::strlen(o), o) == 0) {
					op = o;
					break;
				}
			if (!op) {
				char msg[64];
				if (std::isprint(static_cast<unsigned char>(c)))
					std::snprintf(msg, sizeof msg, "unexpected character '%c'", c);
				else
					std::snprintf(msg, sizeof msg, "unexpected character 0x%02x",
					              static_cast<unsigned char>(c));
				i++;
				push(T::Bad, msg, line, b);
				continue;
			}
			if (op[0] == '(')
				depth++;
			else if (op[0] == ')' && depth > 0)
				depth--;
			i += std::strlen(op);
			push(T::Op, op, line, b);
		}
	}
	push(T::Eof, "", line, i);
}

const Parser::Tok& Parser::at(size_t k) const
{
	return k < toks_.size() ? toks_[k] : toks_.back();
}

void Parser::advance()
{
	if (pos_ + 1 < toks_.size())
		pos_++;
}

void Parser::syntax(const std::string& what) const
{
	const Tok& t = tok();
	if (t.type == T::Bad)
		throw SyntaxError(t.text, t.line, t.text == "unterminated comment");
	std::string found;
	switch (t.type) {
	case T::Eof: found = "end of input"; break;
	case T::Sep: found = "end of statement"; break;
	default: found = "'" + t.text + "'"; break;
	}
	std::string msg = "syntax error: " + what + ", found " + found;
	if (t.type == T::Op && t.text == "=")
		msg += " (use == to compare)";
	// at the end of the input: more lines may complete the statement
	throw SyntaxError(msg, t.line, t.type == T::Eof);
}

void Parser::expectOp(const char* op, const char* what)
{
	if (!isOp(op))
		syntax(what);
	advance();
}

// ------------------------------------------------------------ statements

std::optional<Stmt> Parser::next()
{
	stmtStart_ = pos_;
	if (tok().type == T::Eof)
		return std::nullopt;
	Stmt s = statement();
	if (isOp("}"))
		syntax("unexpected '}'");
	// the statement's text, as typed (without its separator)
	if (pos_ > stmtStart_)
		s.source = src_.substr(at(stmtStart_).begin, at(pos_ - 1).end - at(stmtStart_).begin);
	endStatement(s);
	return s;
}

void Parser::endStatement(Stmt& s)
{
	if (tok().type == T::Sep) {
		s.show = tok().text != ";";
		advance();
	} else if (tok().type == T::Eof || isOp("}")) {
		s.show = true;
	} else {
		syntax("expected end of statement");
	}
}

void Parser::recover()
{
	int depth = 0;
	for (size_t i = stmtStart_; i < toks_.size(); i++) {
		const Tok& t = toks_[i];
		if (t.type == T::Op && t.text == "{")
			depth++;
		else if (t.type == T::Op && t.text == "}" && depth > 0)
			depth--;
		if (t.type == T::Eof) {
			pos_ = i;
			return;
		}
		if (i >= pos_ && t.type == T::Sep && depth == 0) {
			pos_ = i + 1;
			return;
		}
	}
	pos_ = toks_.size() - 1;
}

Expr Parser::parenCondition()
{
	expectOp("(", "expected '('");
	Expr c = expr();
	expectOp(")", "expected ')'");
	return c;
}

void Parser::skipNewlines()
{
	while (tok().type == T::Sep && tok().text == "\n")
		advance();
}

std::vector<Stmt> Parser::block()
{
	skipNewlines(); // the '{' may be on the next line
	int open = tok().line;
	expectOp("{", "expected '{'");
	std::vector<Stmt> body;
	for (;;) {
		while (tok().type == T::Sep)
			advance();
		if (isOp("}")) {
			advance();
			return body;
		}
		if (tok().type == T::Eof)
			throw SyntaxError("missing '}' for the block opened on line " +
			                  std::to_string(open), open, true);
		Stmt s = statement();
		endStatement(s);
		body.push_back(std::move(s));
	}
}

void Parser::elseChain(Stmt& s)
{
	while (isKw("else")) {
		advance();
		if (isKw("if")) {
			advance();
			s.conds.push_back(parenCondition());
			s.blocks.push_back(block());
		} else {
			s.blocks.push_back(block());
			s.hasElse = true;
			return;
		}
		size_t j = pos_;
		while (at(j).type == T::Sep && at(j).text == "\n")
			j++;
		if (!(at(j).type == T::Kw && at(j).text == "else"))
			return;
		pos_ = j;
	}
}

Stmt Parser::statement()
{
	Stmt s;
	s.line = tok().line;
	if (tok().type == T::Sep || tok().type == T::Eof || isOp("}"))
		return s; // empty
	if (tok().type == T::Bad)
		syntax("");
	if (isKw("quit") || isKw("halt")) {
		advance();
		s.type = Stmt::Type::Quit;
		return s;
	}
	if (isKw("if")) {
		s.type = Stmt::Type::If;
		advance();
		s.conds.push_back(parenCondition());
		s.blocks.push_back(block());
		// 'else' may follow on the next line
		size_t j = pos_;
		while (at(j).type == T::Sep && at(j).text == "\n")
			j++;
		if (at(j).type == T::Kw && at(j).text == "else") {
			pos_ = j;
			elseChain(s);
		}
		return s;
	}
	if (isKw("else")) {
		// an else that could not be attached here: it continues the if
		// statement executed just before (typed interactively)
		s.type = Stmt::Type::Else;
		elseChain(s);
		return s;
	}
	if (isKw("while")) {
		s.type = Stmt::Type::While;
		advance();
		s.expr = parenCondition();
		s.blocks.push_back(block());
		return s;
	}
	if (isKw("do")) {
		s.type = Stmt::Type::DoWhile;
		advance();
		s.blocks.push_back(block());
		skipNewlines();
		if (!isKw("while"))
			syntax("expected 'while' after the body of 'do'");
		advance();
		s.expr = parenCondition();
		return s;
	}
	if (isKw("for")) {
		s.type = Stmt::Type::For;
		advance();
		expectOp("(", "expected '('");
		if (tok().type != T::Id)
			syntax("expected a loop variable");
		s.name = tok().text;
		advance();
		expectOp("=", "expected '='");
		s.expr = expr();
		if (tok().type == T::Sep && tok().text == ";") {
			// C-style: for (i = start; condition; i = next)
			s.cStyle = true;
			advance();
			s.stop = expr();
			if (!(tok().type == T::Sep && tok().text == ";"))
				syntax("expected ';'");
			advance();
			if (tok().type != T::Id || tok().text != s.name)
				syntax("the step of a for loop must assign the loop variable " + s.name);
			advance();
			expectOp("=", "expected '='");
			s.step = expr();
			expectOp(")", "expected ')'");
			s.blocks.push_back(block());
			return s;
		}
		if (!isKw("to"))
			syntax("expected 'to' or ';'");
		advance();
		s.stop = sum();
		if (isKw("step")) {
			advance();
			s.step = sum();
		} else {
			s.step = mkInt(1);
		}
		expectOp(")", "expected ')'");
		s.blocks.push_back(block());
		return s;
	}
	if (tok().type == T::Kw)
		throw SyntaxError("'" + tok().text + "' is a reserved word and cannot be "
		                  "used as a name", tok().line);

	Expr e = expr();
	if (!isOp("=") && !isOp(":=")) {
		s.type = Stmt::Type::Expr;
		s.expr = e;
		return s;
	}
	s.type = isOp("=") ? Stmt::Type::Assign : Stmt::Type::Define;
	if (e->kind == Kind::Sym) {
		s.name = e->name;
	} else if (e->kind == Kind::Call) {
		s.name = e->name;
		s.isFunction = true;
		for (const Expr& a : e->args) {
			if (a->kind != Kind::Sym)
				throw SyntaxError("the parameters of " + e->name +
				                  "() must be names", s.line);
			for (const std::string& p : s.params)
				if (p == a->name)
					throw SyntaxError("parameter '" + p + "' appears twice", s.line);
			s.params.push_back(a->name);
		}
	} else {
		throw SyntaxError("can only assign to a name, or define a function "
		                  "such as f(x)", tok().line);
	}
	advance();
	s.expr = expr();
	return s;
}

// ----------------------------------------------------------- expressions

Expr Parser::expr()
{
	Expr a = andExpr();
	while (isOp("||")) {
		advance();
		a = mkCall("||", {a, andExpr()});
	}
	return a;
}

Expr Parser::andExpr()
{
	Expr a = notExpr();
	while (isOp("&&")) {
		advance();
		a = mkCall("&&", {a, notExpr()});
	}
	return a;
}

Expr Parser::notExpr()
{
	if (isOp("!")) {
		advance();
		return mkCall("!", {notExpr()});
	}
	return relExpr();
}

Expr Parser::relExpr()
{
	Expr a = sum();
	for (const char* op : {"==", "!=", "<=", ">=", "<", ">"}) {
		if (isOp(op)) {
			advance();
			Expr b = sum();
			return mkCall(op, {a, b});
		}
	}
	return a;
}

Expr Parser::sum()
{
	Expr a = term();
	while (isOp("+") || isOp("-")) {
		bool plus = isOp("+");
		advance();
		Expr b = term();
		a = plus ? rawAdd(a, b) : rawSub(a, b);
	}
	return a;
}

Expr Parser::term()
{
	Expr a = unary();
	while (isOp("*") || isOp("/")) {
		bool times = isOp("*");
		advance();
		Expr b = unary();
		a = times ? rawMul(a, b) : rawDiv(a, b);
	}
	return a;
}

Expr Parser::unary()
{
	if (isOp("-")) {
		advance();
		Expr a = unary();
		if (a->kind == Kind::Num) // keep literals like -3 compact
			return mkNum(-a->num);
		return rawNeg(a);
	}
	if (isOp("+")) {
		advance();
		return unary();
	}
	return power();
}

Expr Parser::power()
{
	Expr b = primary();
	if (isOp("^")) {
		advance();
		return mkPow(b, unary());
	}
	return b;
}

Expr Parser::argument()
{
	if (tok().type == T::Id && at(pos_ + 1).type == T::Op && at(pos_ + 1).text == "=") {
		std::string name = tok().text;
		advance();
		advance();
		Expr v = expr();
		if (!isKw("to"))
			return mkArg(name, v);
		advance();
		Expr stop = sum();
		Expr step = mkInt(1);
		if (isKw("step")) {
			advance();
			step = sum();
		}
		return mkRange(name, v, stop, step);
	}
	return expr();
}

Expr Parser::primary()
{
	if (tok().type == T::Num) {
		Expr n = mkNum(Rational::fromDecimal(tok().text));
		advance();
		return n;
	}
	if (tok().type == T::Id) {
		std::string name = tok().text;
		advance();
		if (!isOp("("))
			return mkSym(name);
		advance();
		Exprs args;
		if (!isOp(")")) {
			for (;;) {
				args.push_back(argument());
				if (!isOp(","))
					break;
				advance();
			}
		}
		expectOp(")", "expected ',' or ')' in argument list");
		return mkCall(name, std::move(args));
	}
	if (isOp("(")) {
		advance();
		Expr e = expr();
		expectOp(")", "expected ')'");
		return e;
	}
	if (tok().type == T::Str) {
		Expr e = mkStr(tok().text);
		advance();
		return e;
	}
	if (tok().type == T::Kw)
		throw SyntaxError("'" + tok().text + "' is a reserved word and cannot be "
		                  "used as a name", tok().line);
	syntax("expected a number, name or '('");
}

bool textIncomplete(const std::string& s)
{
	size_t n = s.size();
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
		n--;
	if (n > 0 && s[n - 1] == '\\')
		return true;
	try {
		Parser p(s);
		while (p.next()) {
		}
	} catch (const SyntaxError& e) {
		return e.atEof;
	}
	return false;
}

} // namespace xyz
