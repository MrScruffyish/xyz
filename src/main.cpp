// xyz - a small computer algebra system that uses bcl (the library form of
// Gavin D. Howard's bc) for exact arbitrary-precision arithmetic.
//
// Copyright (C) 2026 Gunnar Lindholm
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "interpreter.hpp"

#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* const kVersion = "0.1.0";

void usage(std::ostream& out)
{
	out << "usage: xyz [-h] [-v] [-e expr]... [file]...\n"
	       "\n"
	       "Reads statements from the files (or standard input). A statement\n"
	       "prints its result unless it ends with ';'.\n"
	       "\n"
	       "  -e, --expression EXPR  run EXPR (may be repeated); standard input\n"
	       "                         is then only read if '-' is given as a file\n"
	       "  -h, --help             show this help\n"
	       "  -v, --version          show version and license\n"
	       "\n"
	       "  a = expr      store the value now      f(x) = expr   function, body now\n"
	       "  a := expr     store the formula        f(x) := expr  function, body kept\n"
	       "  if (c) {...} else if (c) {...} else {...}    while (c) {...}\n"
	       "  do {...} while (c)     for (i = 1 to 10 step 1) {...}\n"
	       "\n"
	       "Functions: eval(e, x=v, ...), subs(e, x=v, ...), expand(e), num(e[, digits]),\n"
	       "  diff(e, x[, n]), deg(e, x), coeff(e, x, n), quo(p, q, x), rem(p, q, x),\n"
	       "  gcd(a, b, ...), lcm(a, b, ...), cancel(e)\n"
	       "Statements: disp(e, ...), def(a), unset(a, ...),\n"
	       "  table(e, ..., x = 1 to 10 [step s], ...), quit\n";
}

void version()
{
	std::cout << "xyz " << kVersion << "\n"
	          << "Copyright (C) 2026 Gunnar Lindholm\n"
	             "License GPLv3+: GNU GPL version 3 or later "
	             "<https://gnu.org/licenses/gpl.html>.\n"
	             "This is free software: you are free to change and redistribute it.\n"
	             "There is NO WARRANTY, to the extent permitted by law.\n"
	             "\n"
	             "Arithmetic by bcl from bc, Copyright (c) 2018-2026 Gavin D. Howard\n"
	             "and contributors, BSD 2-Clause License (see LICENSES/).\n";
}

std::string readAll(std::istream& in)
{
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

// Interactive: run each complete line (or group of lines) as it is typed.
int runInteractive(xyz::Interpreter& interp)
{
	int errors = 0;
	std::string chunk, line;
	interp.setShowLines(false);
	while (!interp.quit() && std::getline(std::cin, line)) {
		chunk += line + "\n";
		if (xyz::textIncomplete(chunk))
			continue;
		errors += interp.run(chunk);
		chunk.clear();
	}
	if (!chunk.empty() && !interp.quit())
		errors += interp.run(chunk);
	return errors;
}

int realMain(int argc, char** argv)
{
	std::vector<std::string> exprs, files;
	bool optsDone = false;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (optsDone || a == "-" || a.empty() || a[0] != '-') {
			files.push_back(a);
		} else if (a == "-h" || a == "--help") {
			usage(std::cout);
			return 0;
		} else if (a == "-v" || a == "--version") {
			version();
			return 0;
		} else if (a == "-e" || a == "--expression") {
			if (++i >= argc) {
				std::cerr << "xyz: option " << a << " needs an argument\n";
				return 2;
			}
			exprs.push_back(argv[i]);
		} else if (a.rfind("--expression=", 0) == 0) {
			exprs.push_back(a.substr(13));
		} else if (a == "--") {
			optsDone = true;
		} else {
			std::cerr << "xyz: unknown option " << a << "\n";
			usage(std::cerr);
			return 2;
		}
	}

	xyz::BclSession bcl;
	int errors = 0;
	{
		xyz::Interpreter interp(std::cout, std::cerr);
		for (const std::string& e : exprs) {
			if (interp.quit())
				break;
			errors += interp.run(e);
		}
		if (files.empty() && exprs.empty()) {
			if (isatty(STDIN_FILENO))
				errors += runInteractive(interp);
			else
				errors += interp.run(readAll(std::cin));
		}
		for (const std::string& f : files) {
			if (interp.quit())
				break;
			if (f == "-") {
				errors += interp.run(readAll(std::cin));
				continue;
			}
			std::ifstream in(f);
			if (!in) {
				std::cerr << "xyz: cannot open " << f << "\n";
				errors++;
				continue;
			}
			errors += interp.run(readAll(in));
		}
	} // all numbers must be freed before bcl shuts down
	return errors ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
	try {
		return realMain(argc, argv);
	} catch (const std::bad_alloc&) {
		std::cerr << "xyz: out of memory\n";
	} catch (const std::exception& e) {
		std::cerr << "xyz: fatal: " << e.what() << "\n";
	}
	return 2;
}
