#!/usr/bin/env python3
# Command-line interface tests for xyz: pipes, files, -e, options.
# Copyright (C) 2026 Gunnar Lindholm
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import pty
import subprocess
import sys
import tempfile

failures = 0


def run(binary, args, stdin=""):
    p = subprocess.run([binary] + args, input=stdin, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True, timeout=60)
    return p.stdout, p.stderr, p.returncode


def run_tty(binary, text):
    """Runs xyz with a pseudo-terminal as stdin (interactive mode)."""
    master, slave = pty.openpty()
    p = subprocess.Popen([binary], stdin=slave, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True)
    os.close(slave)
    os.write(master, text.encode())
    os.write(master, b"\x04")  # end of input (Ctrl-D)
    out, _ = p.communicate(timeout=60)
    os.close(master)
    return out


def check(name, got, want):
    global failures
    if got != want:
        failures += 1
        print(f"FAIL {name}\n  expected {want!r}\n  got      {got!r}")


def main():
    b = sys.argv[1] if len(sys.argv) > 1 else "./xyz"
    with tempfile.TemporaryDirectory() as d:
        f1 = os.path.join(d, "one.xyz")
        f2 = os.path.join(d, "two.xyz")
        with open(f1, "w") as f:
            f.write("a := x+1;\n# comment\nexpand(a^2)\n")
        with open(f2, "w") as f:
            f.write("x = 2;\na\n1/0\n")

        check("pipe", run(b, [], "a=4; b=a*a+3;\nb\n"), ("19\n", "", 0))
        check("pipe without trailing newline", run(b, [], "1+1"), ("2\n", "", 0))
        check("empty input", run(b, [], ""), ("", "", 0))
        check("-e", run(b, ["-e", "expand((x+2)^2)"]), ("x^2+4*x+4\n", "", 0))
        check("--expression=", run(b, ["--expression=2^10"]), ("1024\n", "", 0))
        check("several -e share variables",
              run(b, ["-e", "a := x;", "-e", "b := a*a+3;", "-e", "eval(b, x=1)"]),
              ("4\n", "", 0))
        check("-e does not read stdin", run(b, ["-e", "1"], "2\n"), ("1\n", "", 0))
        check("-e then stdin with -", run(b, ["-e", "a = 5;", "-"], "a*2\n"),
              ("10\n", "", 0))
        check("files share variables", run(b, [f1, f2]),
              ("x^2+2*x+1\n3\n", "error: line 3: division by zero\n", 1))
        check("-e before files", run(b, ["-e", "a := y;", f2]),
              ("y\n", "error: line 3: division by zero\n", 1))
        check("quit in -e stops everything", run(b, ["-e", "disp(1); quit", f1]),
              ("1\n", "", 0))
        check("missing file", run(b, [os.path.join(d, "nope")])[1:],
              (f"xyz: cannot open {os.path.join(d, 'nope')}\n", 1))
        check("unknown option exit status", run(b, ["-z"])[2], 2)
        check("-e needs an argument", run(b, ["-e"])[1:],
              ("xyz: option -e needs an argument\n", 2))
        out, err, code = run(b, ["--help"])
        check("--help", (out.startswith("usage: xyz"), err, code), (True, "", 0))
        out, err, code = run(b, ["-v"])
        check("--version mentions licenses",
              ("GPL" in out and "BSD 2-Clause" in out, code), (True, 0))
        check("-- ends options", run(b, ["--", f1]), ("x^2+2*x+1\n", "", 0))
        check("assignments echo unless ';'", run(b, [], "a = 2\nb = 3;\nb\n"),
              ("a = 2\n3\n", "", 0))
        # interactive mode: each finished statement runs as soon as it is typed
        check("interactive: unfinished lines wait",
              run_tty(b, "(1+\n2)\nf(x) := (x+\n1)\nf(1)\nif (1)\n{\ndisp(7)\n}\n"),
              "3\nf(x) := x+1\n2\n7\n")
        check("interactive: if, then else typed on later lines (taken)",
              run_tty(b, "a = 2;\nif (a == 2)\n{ b = 3; }\nelse { b = 10; }\nb\n"),
              "3\n")
        check("interactive: if, then else typed on later lines (not taken)",
              run_tty(b, "a = 5;\nif (a == 2)\n{ b = 3; }\nelse { b = 10; }\nb\n"),
              "10\n")
        check("interactive: errors have no line numbers",
              run_tty(b, "1/0\n"), "error: division by zero\n")
        check("errors go to stderr, results to stdout",
              run(b, [], "1\nfoo(1)\n2\n"),
              ("1\n2\n", "error: line 2: unknown function 'foo'\n", 1))

    print(f"cli tests: {'ok' if not failures else f'{failures} FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
