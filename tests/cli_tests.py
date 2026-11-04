#!/usr/bin/env python3
# Command-line interface tests for xyz: pipes, files, -e, options.
# Copyright (C) 2026 Gunnar Lindholm
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import pty
import shutil
import subprocess
import sys
import tempfile
import threading
import time

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


def run_editing(binary, keys):
    """Runs xyz with stdin and stderr on a pseudo-terminal, so that line
    editing is used if xyz was built with readline. Returns stdout."""
    master, slave = pty.openpty()
    env = dict(os.environ, TERM="xterm")
    p = subprocess.Popen([binary], stdin=slave, stdout=subprocess.PIPE,
                         stderr=slave, text=True, env=env)
    os.close(slave)

    def drain():  # read the terminal side like a real terminal would
        try:
            while os.read(master, 4096):
                pass
        except OSError:
            pass
    reader = threading.Thread(target=drain, daemon=True)
    reader.start()
    for line in keys.splitlines(keepends=True):
        time.sleep(0.2)  # let readline switch the terminal to raw mode
        os.write(master, line.encode())
    time.sleep(0.2)
    os.write(master, b"\x04")  # Ctrl-D at the start of a line: end
    out, _ = p.communicate(timeout=60)
    os.close(master)
    return out


def has_readline(binary):
    # the binary links readline if it contains readline's symbols
    with open(binary, "rb") as f:
        return b"rl_outstream" in f.read()


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
        if has_readline(b):
            left, up = "\x1b[D", "\x1b[A"
            check("line editing: left arrow",
                  run_editing(b, "23" + left + "+\n"), "5\n")
            check("line editing: history with up arrow",
                  run_editing(b, "7*6\n" + up + "\n"), "42\n42\n")
            check("line editing: typed keys do not reach stdout",
                  run_editing(b, "a = 2;\na+1\n"), "3\n")
        else:
            print("skipping line editing tests (built without readline)")
        # save() and transcript()
        work, trans = os.path.join(d, "work.xyz"), os.path.join(d, "work.txt")
        session = ("a = 4\nb = a*a+3;\nb\nf(x) := x^2+1\n"
                   "for (i = 1 to 2) {\n  disp(f(i))\n}\n1/0\n"
                   f'save("{work}")\ntranscript("{trans}");\n')
        out, err, code = run(b, [], session)
        check("save prints a confirmation unless ';'",
              out.splitlines()[-1], f"saved 6 statements to {work}")
        with open(work) as f:
            check("save writes the statements as typed", f.read(),
                  "a = 4\nb = a*a+3;\nb\nf(x) := x^2+1\n"
                  "for (i = 1 to 2) {\n  disp(f(i))\n}\n1/0\n")
        with open(trans) as f:
            check("transcript adds the output as comments", f.read(),
                  "a = 4\n# a = 4\nb = a*a+3;\nb\n# 19\n"
                  "f(x) := x^2+1\n# f(x) := x^2+1\n"
                  "for (i = 1 to 2) {\n  disp(f(i))\n}\n# 2\n# 5\n"
                  "1/0\n# error: line 8: division by zero\n")
        want = ("a = 4\n19\nf(x) := x^2+1\n2\n5\n",
                "error: line 8: division by zero\n", 1)
        check("a saved session replays the same", run(b, [work]), want)
        # (the comment lines shift the line numbers in error messages)
        check("a transcript replays the same", run(b, [trans]),
              (want[0], "error: line 13: division by zero\n", 1))
        check("save from -e and interactive input",
              (run(b, ["-e", "x = 1", "-e", f'save("{work}");']),
               open(work).read()), (("x = 1\n", "", 0), "x = 1\n"))
        check("save in interactive mode records what was typed",
              (run_tty(b, f'a := x+1\nif (1)\n{{ c = 2; }}\nsave("{work}");\n'),
               open(work).read()),
              ("a := x+1\n", "a := x+1\nif (1)\n{ c = 2; }\n"))
        # transcript as a LaTeX document
        tex = os.path.join(d, "work.tex")
        run(b, [], "a = 4\nf(x) := x^2/x\nfor (i = 1 to 2) { disp(i, i/2) }\n"
                   f'1/0\ntranscript("{tex}")\n')
        with open(tex) as f:
            doc = f.read()
        check("LaTeX transcript", all(part in doc for part in (
            "\\documentclass{article}", "\\begin{verbatim}\na = 4\n\\end{verbatim}",
            "\\begin{gather*}\na = 4\n\\end{gather*}",
            "f\\left(x\\right) \\mathrel{:=} \\frac{x^{2}}{x}",
            "\\begin{array}{cc}\n1 & \\frac{1}{2} \\\\\n2 & 1\n\\end{array}",
            "\\textit{error: line 4: division by zero}",
            "\\end{document}")), True)
        if shutil.which("pdflatex"):
            p = subprocess.run(["pdflatex", "-interaction=nonstopmode",
                                "-halt-on-error", "work.tex"], cwd=d,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check("LaTeX transcript compiles with pdflatex", p.returncode, 0)
        else:
            print("skipping pdflatex check (not installed)")

        check("--trunc truncates like bc",
              run(b, ["--trunc", "-e", "disp(num(2/3, 5), num(pi, 4), num(-2/3, 3))"]),
              ("0.66666\t3.1415\t-0.666\n", "", 0))
        check("without --trunc num rounds",
              run(b, ["-e", "disp(num(2/3, 5), num(pi, 4), num(-2/3, 3))"]),
              ("0.66667\t3.1416\t-0.667\n", "", 0))

        # --imag=NAME
        check("--imag=i: input and output use i",
              run(b, ["--imag=i", "-e", "disp((3+2i)*(1-i), Re(1/(x+i)), num(i/3, 3))"]),
              ("5-i\tx/(x^2+1)\t0.333*i\n", "", 0))
        check("--imag=i: im is an ordinary name",
              run(b, ["--imag", "i", "-e", "im = 5; im*2i"]), ("10*i\n", "", 0))
        check("--imag=i: i cannot be assigned",
              run(b, ["--imag=i", "-e", "i = 3"])[1],
              "error: line 1: 'i' is the imaginary unit and cannot be assigned\n")
        check("--imag needs a usable name", run(b, ["--imag=if", "-e", "1"])[1:],
              ("xyz: --imag: 'if' cannot be used as a name\n", 2))
        run(b, ["--imag=i", "-e", "a = 2i;", "-e", f'save("{work}");'])
        with open(work) as f:
            check("saved files say which unit they need", f.read(),
                  f"# the imaginary unit is i: run with  xyz --imag=i {work}\n"
                  "a = 2i;\n")
        check("errors go to stderr, results to stdout",
              run(b, [], "1\nfoo(1)\n2\n"),
              ("1\n2\n", "error: line 2: unknown function 'foo'\n", 1))

    print(f"cli tests: {'ok' if not failures else f'{failures} FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
