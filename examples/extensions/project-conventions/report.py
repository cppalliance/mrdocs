#!/usr/bin/env python3
"""Capture the project-conventions extension's warnings as real tool output.

The Corpus Transforms page shows the warnings this example produces. Rather than
hard-code them in prose, where they would drift as the extension or MrDocs'
wording changed, this script runs the example and writes the warning lines to
warnings.txt, which the page includes: next to this script, or in the
directory given by `--output=<dir>`. The build's example test runs it that
way and compares the result with the committed warnings.txt, so CI fails if
the two drift apart.

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
REPORT = "warnings.txt"
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
    output = HERE
    extra = []
    for arg in sys.argv[1:]:
        if arg.startswith("--output="):
            output = arg[len("--output="):]
        else:
            extra.append(arg)
    report = capture(extra)
    if not report:
        sys.exit("no warnings captured; is MRDOCS set and the extension loaded?")
    path = os.path.join(output, REPORT)
    with open(path, "w", newline="\n") as f:
        f.write(report)
    print(f"Wrote {path}")


if __name__ == "__main__":
    main()
