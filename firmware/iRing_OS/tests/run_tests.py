#!/usr/bin/env python3
"""Build and run the firmware's hardware-independent C++ tests."""
import argparse
import os
import shlex
import subprocess
import tempfile
from pathlib import Path
from test_support import ROOT, SOURCES, SUITES, tool

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--suite", choices=SUITES, action="append")
    args = parser.parse_args()
    compiler = shlex.split(args.cxx)
    if not compiler:
        parser.error("--cxx must name a compiler")
    compiler[0] = tool(compiler[0])
    with tempfile.TemporaryDirectory(prefix="iring-tests-") as output:
        for suite in args.suite or SUITES:
            binary = Path(output) / (suite + (".exe" if os.name == "nt" else ""))
            subprocess.run(compiler + ["-std=c++11", *map(str, SOURCES),
                str(ROOT / f"tests/{suite}.cpp"), str(ROOT / "tests/test_main.cpp"),
                "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(f"PASS {suite}", flush=True)

if __name__ == "__main__":
    main()
