#!/usr/bin/env python3
"""Run MrDocs' no-op generator as a documentation linter and let an AI agent
fix what it reports, looping until the check is clean.

The first argument is the config file (default mrdocs.yml); anything after it
is forwarded to mrdocs (lint flags, --max-errors, include directories). The
environment supplies AGENT (the agent's non-interactive command, default
"agent -p -f"; use "claude -p" for Claude Code), MRDOCS (the binary), BATCH
(source locations per round, default 200), and MAX_ROUNDS (default 40). The
agent prompt lives beside this script in prompt.md.
"""

import os
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

MRDOCS = os.environ.get("MRDOCS", "mrdocs")
AGENT = shlex.split(os.environ.get("AGENT", "agent -p -f"))
BATCH = int(os.environ.get("BATCH", "200"))
MAX_ROUNDS = int(os.environ.get("MAX_ROUNDS", "40"))
PROMPT = (Path(__file__).resolve().parent / "prompt.md").read_text(encoding="utf-8")

CONFIG = sys.argv[1] if len(sys.argv) > 1 else "mrdocs.yml"
CMD = [MRDOCS, f"--config={CONFIG}", "--generator=noop", *sys.argv[2:]]

ANSI = re.compile(r"\x1b\[[0-9;]*m")
LOCATION = re.compile(r"^\S.*:\d+:\d+:\s*$")
FOOTER = re.compile(
    r"^(An issue occurred|If you believe|\s*MrDocs Version|\s*Error Location|"
    r"\s*Reported From|with the following details|"
    r"max-errors=|error limit reached)")


def main():
    """Check, repair, repeat until the docs are clean or the rounds run out."""
    print(f"Checking with: {shlex.join(CMD)}")
    timings = []
    for round in range(1, MAX_ROUNDS + 1):
        clean, report, check_time = check()
        if clean:
            print(f"Documentation is clean after {round - 1} round(s).")
            return summary(timings, 0)
        # No parseable diagnostics (a compile error or a broken config) is
        # not "clean" and not "stalled": hand the agent the raw output so it
        # can unblock extraction, and keep going.
        batch = diagnostics(report)[:BATCH] or [report]
        if stalled(batch):
            print("The same locations keep coming back; stopping so a human "
                  "can look.", file=sys.stderr)
            return summary(timings, 1)
        print(f"Round {round}: check took {check_time:.0f}s; "
              f"handing {len(batch)} location(s) to the agent.")
        agent_time = repair(batch)
        print(f"Round {round}: agent took {agent_time:.0f}s.")
        timings.append((check_time, agent_time))
    print(f"Still failing after {MAX_ROUNDS} rounds; stopping so a human "
          f"can look.", file=sys.stderr)
    return summary(timings, 1)


def check():
    """Run the strict no-op check; return (clean?, report, seconds).

    Clean requires both a success exit code and a non-empty corpus: a run
    that extracts zero declarations is a broken configuration, not a clean
    one, even when mrdocs reports success.
    """
    start = time.monotonic()
    run = subprocess.run(CMD, capture_output=True, text=True)
    report = ANSI.sub("", run.stdout + run.stderr)
    clean = run.returncode == 0 and "Extracted 0 declarations" not in report
    return clean, report, time.monotonic() - start


def diagnostics(report):
    """Split the report into its location-grouped diagnostic blocks.

    A block starts with a "path:line:col:" header, then its numbered
    messages and a source snippet. The warn-as-error footer is skipped.
    """
    blocks, current = [], []
    for line in report.splitlines():
        if FOOTER.match(line):
            continue
        if LOCATION.match(line):
            if current:
                blocks.append("\n".join(current))
            current = [line]
        elif current:
            current.append(line)
    if current:
        blocks.append("\n".join(current))
    return [block for block in blocks if block.strip()]


def stalled(batch):
    """True when three rounds in a row report the same locations.

    The batch is identified by its location headers only: with --max-errors
    the total count stays pinned at the cap while the frontier advances, so
    only the same locations coming back means no progress.
    """
    headers = tuple(block.splitlines()[0] for block in batch)
    stalled.streak = stalled.streak + 1 if headers == stalled.previous else 0
    stalled.previous = headers
    return stalled.streak >= 2


stalled.previous = None
stalled.streak = 0


def repair(batch):
    """Hand one rendered prompt to the agent; return how long it took."""
    max_errors = next(
        (arg.split("=", 1)[1] for arg in CMD
         if arg.startswith("--max-errors=")), str(BATCH))
    prompt = PROMPT
    for key, value in (
        ("command", shlex.join(CMD)),
        ("report", "\n\n".join(batch)),
        ("max_errors", max_errors),
        ("batch", str(len(batch))),
    ):
        prompt = prompt.replace("{" + key + "}", value)
    start = time.monotonic()
    subprocess.run(AGENT + [prompt])
    return time.monotonic() - start


def summary(timings, exit_code):
    """Print a per-round timing table with totals and averages."""
    if timings:
        print(f"\n{'Round':>7} {'mrdocs':>8} {'agent':>8}")
        for i, (check_time, agent_time) in enumerate(timings, 1):
            print(f"{i:>7} {check_time:>7.0f}s {agent_time:>7.0f}s")
        checks, agents = zip(*timings)
        print(f"{'Total':>7} {sum(checks):>7.0f}s {sum(agents):>7.0f}s")
        print(f"{'Average':>7} {sum(checks) / len(checks):>7.0f}s "
              f"{sum(agents) / len(agents):>7.0f}s")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
