#!/usr/bin/env python3
"""Capture the project-conventions extension's warnings as real tool output.

The Corpus Transforms page shows the warnings this example produces. Rather than
hard-code them in prose, where they would drift as the extension or MrDocs'
wording changed, this script runs the example and writes the warning lines to
warnings.txt, which the page includes. `--check` re-runs the capture and
compares it to the committed warnings.txt, so CI fails if the two drift apart.

Both the JavaScript and Lua versions of the extension are present, so each
warning is emitted twice; duplicates are collapsed and the lines sorted, so
the file is identical on every platform.

Set MRDOCS to the mrdocs binary if it is not on PATH. Any extra arguments are
forwarded to mrdocs (the build passes its built-in directory options this way).
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MRDOCS = os.environ.get("MRDOCS", "mrdocs")
REPORT = os.path.join(HERE, "warnings.txt")
ANSI = re.compile(r"\x1b\[[0-9;]*m")
WARNING = re.compile(r".*: low-quality brief: .*")


def capture(extra):
    run = subprocess.run(
        [MRDOCS, "--config=mrdocs.yml", "--generator=noop", "--log-level=warn", *extra],
        cwd=HERE,
        capture_output=True, text=True)
    lines = set()
    for raw in (run.stdout + run.stderr).splitlines():
        clean = ANSI.sub("", raw).strip()
        if WARNING.match(clean):
            lines.add(clean)
    # Sorted so the file is stable across platforms: the corpus iteration
    # order (and so the warning order) is not.
    return "".join(line + "\n" for line in sorted(lines))


def main():
    flags = sys.argv[1:]
    extra = [a for a in flags if a not in ("--check", "--write")]
    report = capture(extra)
    if not report:
        sys.exit("no warnings captured; is MRDOCS set and the extension loaded?")

    if "--check" in flags:
        current = open(REPORT).read() if os.path.exists(REPORT) else ""
        if report != current:
            sys.exit("warnings.txt is out of date; run `python report.py` to update it.")
        print("warnings.txt is up to date.")
    else:
        with open(REPORT, "w") as f:
            f.write(report)
        print(f"Wrote {REPORT}")


if __name__ == "__main__":
    main()
