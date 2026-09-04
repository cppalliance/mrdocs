#!/usr/bin/env python3
"""Capture MrDocs' documentation report as real tool output.

The No-op page shows the report the example header produces (it leaves its two
parameters undocumented). Rather than hard-code it in prose, where it would
drift as MrDocs' wording changed, this script runs the real check and writes the
diagnostic messages to report.txt, which the page includes. `--check`
regenerates the report and compares it to the committed report.txt, so CI fails
if the two drift apart.

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
REPORT = os.path.join(HERE, "report.txt")
MESSAGE = re.compile(r"^\s*\d+\)\s+(.*\S)\s*$")


def capture(extra):
    run = subprocess.run(
        [MRDOCS, "--config=mrdocs.yml", *extra],
        cwd=HERE,
        capture_output=True, text=True)
    lines = [m.group(1) for m in map(MESSAGE.match, run.stderr.splitlines()) if m]
    return "".join(line + "\n" for line in lines)


def main():
    flags = sys.argv[1:]
    extra = [a for a in flags if a not in ("--check", "--write")]
    report = capture(extra)
    if not report:
        sys.exit("no diagnostics captured; is MRDOCS set and the check still failing?")

    if "--check" in flags:
        current = open(REPORT).read() if os.path.exists(REPORT) else ""
        if report != current:
            sys.exit("report.txt is out of date; run `python report.py` to update it.")
        print("report.txt is up to date.")
    else:
        with open(REPORT, "w") as f:
            f.write(report)
        print(f"Wrote {REPORT}")


if __name__ == "__main__":
    main()
