#!/usr/bin/env python3
"""Capture MrDocs' documentation report as real tool output.

The No-op page shows the report the example header produces (it leaves its two
parameters undocumented). Rather than hard-code it in prose, where it would
drift as MrDocs' wording changed, this script runs the real check and writes the
diagnostic messages to report.txt, which the page includes: next to this
script, or in the directory given by `--output=<dir>`. The build's example test
runs it that way and compares the result with the committed report.txt, so CI
fails if the two drift apart.

Each diagnostic prints as "    N) <symbol>: <message>" under a source-location
header; the messages are the stable part, so the volatile framing (the absolute
path, the caret, the version footer) is dropped.

Set MRDOCS to the mrdocs binary if it is not on PATH. Any extra arguments are
forwarded to mrdocs (the build passes its built-in directory options this way).
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MRDOCS = os.environ.get("MRDOCS", "mrdocs")
REPORT = "report.txt"
MESSAGE = re.compile(r"^\s*\d+\)\s+(.*\S)\s*$")


def capture(extra):
    run = subprocess.run(
        [MRDOCS, "--config=mrdocs.yml", *extra],
        cwd=HERE,
        capture_output=True, text=True)
    lines = [m.group(1) for m in map(MESSAGE.match, run.stderr.splitlines()) if m]
    return "".join(line + "\n" for line in lines)


def main():
    output = HERE
    extra = []
    for arg in sys.argv[1:]:
        if arg.startswith("--output="):
            output = arg[len("--output="):]
        else:
            extra.append(arg)
    report = capture(extra)
    if not report:
        sys.exit("no diagnostics captured; is MRDOCS set and the check still failing?")
    path = os.path.join(output, REPORT)
    with open(path, "w", newline="\n") as f:
        f.write(report)
    print(f"Wrote {path}")


if __name__ == "__main__":
    main()
