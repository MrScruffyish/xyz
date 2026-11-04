#!/usr/bin/env python3
# Golden-output tests for xyz.
# Copyright (C) 2026 Gunnar Lindholm
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Runs every tests/cases/*.t file. Format of a case file:

    # comments before the first case are ignored
    === name of the test
    input lines fed to xyz on stdin
    ---
    expected output (stdout and stderr interleaved)

A case may use '--- exit N' instead of '---' to also check the exit status.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_cases(path):
    cases, cur, section = [], None, None
    with open(path, encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if line.startswith("=== "):
                cur = {"name": line[4:].strip(), "input": [], "expected": [],
                       "exit": None, "line": lineno}
                cases.append(cur)
                section = "input"
            elif cur is not None and section == "input" and (
                    line == "---" or line.startswith("--- exit")):
                section = "expected"
                if line.startswith("--- exit"):
                    cur["exit"] = int(line.split()[2])
            elif cur is not None:
                cur[section].append(line)
    for c in cases:
        while c["expected"] and c["expected"][-1] == "":
            c["expected"].pop()
    return cases


def run(binary, text):
    p = subprocess.run([binary], input=text, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, timeout=60)
    return p.stdout, p.returncode


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "./xyz"
    only = sys.argv[2] if len(sys.argv) > 2 else None
    files = sorted(os.listdir(os.path.join(HERE, "cases")))
    total = failed = 0
    for fn in files:
        if not fn.endswith(".t"):
            continue
        path = os.path.join(HERE, "cases", fn)
        for c in parse_cases(path):
            if only and only not in c["name"]:
                continue
            total += 1
            out, code = run(binary, "\n".join(c["input"]) + "\n")
            got = out.rstrip("\n").split("\n") if out.strip("\n") else []
            ok = got == c["expected"]
            if c["exit"] is not None and code != c["exit"]:
                ok = False
            if not ok:
                failed += 1
                print(f"FAIL {fn}:{c['line']}: {c['name']}")
                print("  input:    " + "\n            ".join(c["input"]))
                print("  expected: " + "\n            ".join(c["expected"]))
                print("  got:      " + "\n            ".join(got))
                if c["exit"] is not None:
                    print(f"  exit: expected {c['exit']}, got {code}")
    print(f"{total - failed}/{total} golden tests passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
