#!/usr/bin/env python3
"""Drive an AI agent to fix a project's documentation with Mr.Docs, in three
phases that minimize how often Mr.Docs has to run.

Phase 1, compile: run extraction (no warnings, no max-errors) and hand the
agent the compiler errors until the whole project extracts.

Phase 2, document: dump the corpus once with Mr.Docs' JSON generator, derive
the work-list from it (undocumented symbols, missing briefs, anomalous
briefs), group it by file, and hand the agent one file's complete task list
at a time. No Mr.Docs runs between files, so this phase also knows exactly
how much work remains and estimates completion.

Phase 3, warnings: run the strict check without max-errors, collect every
diagnostic, feed them to the agent batch by batch in an inner loop, and only
re-run Mr.Docs when the collected list is exhausted.

The first argument is the config file; anything else is forwarded to every
mrdocs run. Options (--mrdocs, --agent, --parallel, --batch, --max-rounds,
--cycles, --state) each fall back to an environment variable (MRDOCS, AGENT,
PARALLEL, BATCH, MAX_ROUNDS, CYCLES, STATE) then a default. Run with --help
for the full list. The caps bound mrdocs re-runs, never the per-file or
per-batch fan-out between runs.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from threading import Lock

HERE = Path(__file__).resolve().parent


def default_state_path(config):
    """Checkpoint under the system temp dir, keyed by the config's absolute
    path. It persists across interruptions so a resumed run finds it, but
    never lands in the project tree where it would clutter git status."""
    slug = re.sub(r"[^A-Za-z0-9]+", "-", str(Path(config).resolve())).strip("-")
    return Path(tempfile.gettempdir()) / "fix-docs" / f"{slug}.json"


def parse_args():
    """Parse the command line. Every option falls back to its environment
    variable, then to a default. Any argument we do not recognize is
    forwarded verbatim to every mrdocs invocation (include dirs, addons, and
    so on). allow_abbrev is off so a forwarded flag is never mistaken for a
    prefix of one of ours."""
    env = os.environ.get
    p = argparse.ArgumentParser(
        prog="fix-docs", allow_abbrev=False,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Drive an AI agent to document a project with Mr.Docs.",
        epilog="Any other arguments are forwarded to every mrdocs run.")
    p.add_argument("config", nargs="?", default="mrdocs.yml",
                   help="the mrdocs config file (default: mrdocs.yml)")
    p.add_argument("--mrdocs", default=env("MRDOCS", "mrdocs"),
                   help="mrdocs binary (env MRDOCS; default: mrdocs on PATH)")
    p.add_argument("--agent", default=env("AGENT", "agent -p -f"),
                   help="agent command; the prompt is appended as its last "
                        "argument (env AGENT; default: 'agent -p -f')")
    p.add_argument("--parallel", type=int, default=int(env("PARALLEL", "1")),
                   help="concurrent agents in the document phase "
                        "(env PARALLEL; default: 1)")
    p.add_argument("--batch", type=int, default=int(env("BATCH", "30")),
                   help="diagnostics per agent call in the warnings phase "
                        "(env BATCH; default: 30)")
    p.add_argument("--max-rounds", type=int,
                   default=int(env("MAX_ROUNDS", "40")),
                   help="mrdocs re-run cap in the compile and warnings phases "
                        "(env MAX_ROUNDS; default: 40)")
    p.add_argument("--cycles", type=int, default=int(env("CYCLES", "3")),
                   help="dump/fan-out passes in the document phase "
                        "(env CYCLES; default: 3)")
    p.add_argument("--state", default=env("STATE"),
                   help="checkpoint path (env STATE; default: under the "
                        "system temp dir, keyed by the config path)")
    return p.parse_known_args()


ARGS, EXTRA = parse_args()
MRDOCS = ARGS.mrdocs
AGENT = shlex.split(ARGS.agent)
PARALLEL = ARGS.parallel
BATCH = ARGS.batch
MAX_ROUNDS = ARGS.max_rounds
CYCLES = ARGS.cycles
CONFIG = ARGS.config
EXTRA = [a for a in EXTRA if not a.startswith("--max-errors")]
STATE_PATH = Path(ARGS.state) if ARGS.state else default_state_path(CONFIG)

# The strict lint set for phase 3. Phases 1 and 2 run with warnings off.
STRICT = [
    "--warnings", "--warn-if-undocumented", "--warn-no-paramdoc",
    "--warn-unnamed-param", "--warn-if-undoc-enum-val", "--warn-if-doc-error",
    "--warn-broken-ref", "--warn-if-undocumented-namespace",
    "--warn-no-brief", "--warn-as-error",
]

ANSI = re.compile(r"\x1b\[[0-9;]*m")
LOCATION = re.compile(r"^\S.*:\d+:\d+:\s*$")
FOOTER = re.compile(
    r"^(An issue occurred|If you believe|\s*MrDocs Release|\s*MrDocs Version|\s*Error Location|"
    r"\s*Reported From|with the following details|error limit reached)")


def main():
    print(f"config={CONFIG} agent={shlex.join(AGENT)} parallel={PARALLEL}")
    corpus = phase_compile()
    if corpus is None:
        return 1
    if not phase_document(corpus):
        return 1
    if not phase_warnings():
        return 1
    print("All three phases are clean.")
    return 0


# -------- shared helpers

def run_mrdocs(args):
    """Run mrdocs; return (returncode, ansi-stripped combined output)."""
    cmd = [MRDOCS, f"--config={CONFIG}", *EXTRA, *args]
    run = subprocess.run(cmd, capture_output=True, text=True)
    return run.returncode, ANSI.sub("", run.stdout + run.stderr), cmd


def run_agent(prompt):
    """Hand one rendered prompt to the agent; return (seconds, exit code).
    A non-zero exit means the agent invocation itself failed (crash, bad
    command, rate-limit abort), not that it ran but left work undone."""
    start = time.monotonic()
    result = subprocess.run(AGENT + [prompt])
    return time.monotonic() - start, result.returncode


# Printed when every agent in a phase failed to run. The work is real, so the
# problem is the agent command, not the project: stop before the next phase so
# the user can point --agent (or AGENT) at another tool.
def agent_broken(phase):
    print(f"[{phase}] every agent invocation failed (non-zero exit). The "
          f"agent command looks broken or unavailable; stopping so you can "
          f"switch agents (--agent / AGENT). Later phases will not run.",
          file=sys.stderr)


def render(name, **values):
    text = (HERE / "prompts" / f"{name}.md").read_text(encoding="utf-8")
    for key, value in values.items():
        text = text.replace("{" + key + "}", str(value))
    return text


def load_state():
    if STATE_PATH.exists():
        return json.loads(STATE_PATH.read_text(encoding="utf-8"))
    return {"documented": []}


def save_state(state):
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    STATE_PATH.write_text(json.dumps(state, indent=1), encoding="utf-8")


# -------- phase 1: fix until it compiles

def phase_compile():
    """Extract with the JSON generator until the project compiles. Returns
    the corpus from the successful run so phase 2 can reuse it instead of
    extracting a second time, or None if it never compiled."""
    for round in range(1, MAX_ROUNDS + 1):
        code, corpus, out, cmd = extract_json()
        errors = compile_errors(out)
        if corpus is not None and corpus.get("symbols") and not errors:
            print(f"[compile] clean after {round - 1} repair round(s).")
            return corpus
        if not errors:
            errors = [out.strip()[-4000:] or "(no output)"]
        print(f"[compile] round {round}: {len(errors)} error line(s); "
              f"handing them to the agent.")
        seconds, agent_code = run_agent(render(
            "compile",
            command=shlex.join(cmd),
            config=CONFIG,
            errors="\n".join(errors[:400])))
        if agent_code != 0:
            agent_broken("compile")
            return None
        print(f"[compile] agent took {seconds:.0f}s.")
    print("[compile] out of rounds.", file=sys.stderr)
    return None


def compile_errors(out):
    """Compiler diagnostics and the failure summary, one line each."""
    return [line for line in out.splitlines()
            if re.search(r"(fatal error|error):", line)
            or "errors occurred" in line
            or "Failed to run action" in line]


# -------- phase 2: fix until everything is documented and not weird

def phase_document(initial_corpus=None):
    state = load_state()
    for cycle in range(1, CYCLES + 1):
        # The compile phase already extracted; reuse its corpus for cycle 1
        # instead of extracting again just to say the same thing.
        if cycle == 1 and initial_corpus is not None:
            tasks = work_list_from(initial_corpus)
        else:
            tasks = dump_work_list()
        done = set(map(tuple, state["documented"]))
        by_file = {}
        for t in tasks:
            key = (t["file"], t["name"], t["reason"])
            if key not in done:
                by_file.setdefault(t["file"], []).append(t)
        if not by_file:
            print(f"[document] work-list clean after cycle {cycle - 1}.")
            return True

        # Do the files nearest the top of the index first, so an interrupted
        # run still leaves everything a reader sees first fully documented. A
        # file's priority is the best index rank among its tasks; the tasks
        # are already sorted by rank, so the first one is that best.
        ordered_files = sorted(
            by_file.items(), key=lambda kv: index_rank(kv[1][0]))
        total_files = len(by_file)
        total_tasks = sum(len(v) for v in by_file.values())
        print(f"[document] cycle {cycle}: {total_tasks} task(s) across "
              f"{total_files} file(s).")

        lock = Lock()
        finished = [0]
        failed = [0]
        started = time.monotonic()

        def fix_file(file, file_tasks):
            file_tasks.sort(key=lambda t: -t["line"])
            listing = "\n".join(
                f"- line ~{t['line']}: {t['kind']} `{t['name']}`: {t['reason']}"
                + (f"\n  current brief: \"{t['brief']}\"" if t.get("brief") else "")
                for t in file_tasks)
            seconds, code = run_agent(render("document", file=file, tasks=listing))
            with lock:
                finished[0] += 1
                if code == 0:
                    # Only a clean agent run counts as done; a failed one is
                    # left off the checkpoint so it is retried.
                    for t in file_tasks:
                        state["documented"].append(
                            [t["file"], t["name"], t["reason"]])
                    save_state(state)
                else:
                    failed[0] += 1
                # ETA from wall-clock throughput, not per-file time: with
                # several agents in flight, files finish faster than any one
                # takes, so elapsed/finished already folds in the parallelism.
                elapsed = time.monotonic() - started
                rate = elapsed / finished[0]
                eta = rate * (total_files - finished[0])
                note = " FAILED" if code != 0 else ""
                print(f"[document] {finished[0]}/{total_files} files{note} "
                      f"({len(file_tasks)} task(s) in {seconds:.0f}s, "
                      f"{rate:.0f}s/file wall, ~{eta / 60:.0f}min left)")

        if PARALLEL > 1:
            with ThreadPoolExecutor(max_workers=PARALLEL) as pool:
                futures = [pool.submit(fix_file, f, ts)
                           for f, ts in ordered_files]
                for future in as_completed(futures):
                    future.result()
        else:
            for f, ts in ordered_files:
                fix_file(f, ts)

        if failed[0] == total_files:
            agent_broken("document")
            return False
        if failed[0]:
            print(f"[document] {failed[0]}/{total_files} file(s) had a "
                  f"failing agent; they will be retried next cycle.")

        # The checkpoint only protects a cycle against interruption. Once a
        # cycle completes, the next dump is the ground truth: a task that
        # still shows up was not really fixed and must be queued again.
        state["documented"] = []
        save_state(state)
    print(f"[document] work-list still not empty after {CYCLES} cycles.",
          file=sys.stderr)
    return False


def extract_json():
    """Run the JSON generator once. Returns (returncode, corpus|None,
    output, cmd); corpus is None when no reference.json was produced."""
    with tempfile.TemporaryDirectory(prefix="fix-docs-") as out:
        code, text, cmd = run_mrdocs([
            "--generator=json", f"--output={out}",
            "--warnings=false", "--log-level=warn"])
        path = Path(out) / "reference.json"
        corpus = json.loads(path.read_text(encoding="utf-8")) \
            if path.exists() else None
    return code, corpus, text, cmd


def dump_work_list():
    """Extract with the JSON generator and derive the work-list."""
    code, corpus, text, _ = extract_json()
    if corpus is None:
        sys.exit(f"the json generator produced no output "
                 f"(exit {code}):\n{text.strip()[-2000:]}")
    return work_list_from(corpus)


def work_list_from(corpus):
    """Derive the work-list from a corpus, in index order."""
    symbols = corpus.get("symbols", [])
    by_id = {s["id"]: s for s in symbols if "id" in s}
    tasks = [t for s in symbols for t in tasks_for(s, by_id)]
    tasks.sort(key=index_rank)
    return tasks


def index_rank(task):
    """Sort key placing a task where its symbol sits in the index: shallower
    namespaces first (the global namespace, then one level, then two...),
    alphabetical within a level."""
    return (task["depth"], task["name"].lower())


def tasks_for(sym, by_id):
    """Every work-list entry a symbol needs (zero or more).

    These mirror the offline-computable subset of the strict warnings, so
    the warnings phase has almost nothing left to catch: undocumented
    symbols (namespaces included), missing or anomalous briefs, function
    parameters with no doc, and undocumented enum values. Only regular
    (documented-surface) symbols are considered. The global namespace, which
    has no name and no location, drops out on its own below.
    """
    if sym.get("extraction", "regular") != "regular":
        return []
    kind = sym.get("kind", "?")
    name = qualified_name(sym, by_id)
    tasks = classify(sym, by_id, kind, name)
    # Every task from one symbol shares its index depth (the enum-constant's
    # namespaces are the same as its enum's). Stamp it once for sorting.
    depth = namespace_depth(sym, by_id)
    for task in tasks:
        task["depth"] = depth
    return tasks


def classify(sym, by_id, kind, name):
    """The reasons a symbol needs work, as unranked tasks."""
    # Enum values are separate symbols with no location of their own, so
    # report them at the parent enum. Only when the enum itself is
    # documented: a wholly-undocumented enum stays one task, and its values
    # get written when the agent documents it.
    if kind == "enum-constant":
        parent = by_id.get(sym.get("parent"))
        if sym.get("doc") or not (parent and parent.get("doc")):
            return []
        loc = symbol_loc(parent)
        return [make_task(loc, name, kind, "undocumented enum value")] \
            if loc else []

    loc = symbol_loc(sym)
    if not loc:
        return []

    if not sym.get("doc"):
        # The whole symbol is undocumented; the agent writes its brief,
        # params, and enum values in one comment, so no detail tasks here.
        return [make_task(loc, name, kind, "undocumented")]

    tasks = []
    brief = brief_text(sym)
    if not brief:
        tasks.append(make_task(loc, name, kind, "no-brief"))
    else:
        anomaly = brief_anomaly(brief)
        if anomaly:
            tasks.append(make_task(loc, name, kind,
                                   f"anomalous-brief: {anomaly}", brief))

    # A documented function whose parameters are not fully documented: named
    # ones with no @param, and unnamed ones (which must first be given a name
    # in the signature before they can be documented at all).
    if kind == "function":
        missing = missing_param_docs(sym)
        if missing:
            tasks.append(make_task(loc, name, kind,
                                   "missing param doc: " + ", ".join(missing)))
        unnamed = sum(1 for p in sym.get("params", []) if not p.get("name"))
        if unnamed:
            tasks.append(make_task(loc, name, kind,
                                   f"unnamed parameter(s): {unnamed}"))
    return tasks


def namespace_depth(sym, by_id):
    """How many named namespaces enclose a symbol: 0 in the global namespace,
    1 one level in, and so on. Macros and other global symbols are 0."""
    depth = 0
    cur = by_id.get(sym.get("parent"))
    while cur is not None:
        if cur.get("kind") == "namespace" and cur.get("name"):
            depth += 1
        cur = by_id.get(cur.get("parent"))
    return depth


def symbol_loc(sym):
    """A symbol's definition location, or None when it has no usable one."""
    loc = (sym.get("loc") or {}).get("defLoc") \
        or ((sym.get("loc") or {}).get("loc") or [None])[0]
    if loc and loc.get("sourcePath") and loc.get("lineNumber"):
        return loc
    return None


def make_task(loc, name, kind, reason, brief=None):
    task = {
        "file": loc["sourcePath"],
        "line": int(loc["lineNumber"]),
        "name": name,
        "kind": kind,
        "reason": reason,
    }
    if brief:
        task["brief"] = brief[:200] + ("..." if len(brief) > 200 else "")
    return task


def missing_param_docs(sym):
    """Named parameters of a function that have no @param entry."""
    documented = {p.get("name")
                  for p in (sym.get("doc") or {}).get("params", [])}
    return [p["name"] for p in sym.get("params", [])
            if p.get("name") and p["name"] not in documented]


def qualified_name(sym, by_id):
    """A symbol's qualified name, built by walking parent ids."""
    parts = []
    cur = sym
    while cur and cur.get("name"):
        parts.append(cur["name"])
        cur = by_id.get(cur.get("parent"))
    return "::".join(reversed(parts))


def brief_text(sym):
    """A symbol's brief as trimmed plain text, or "" when it has none."""
    brief = (sym.get("doc") or {}).get("brief")

    def walk(node):
        text = node.get("literal", "") if isinstance(node, dict) else ""
        for child in (node.get("children") or []) if isinstance(node, dict) else []:
            text += walk(child)
        return text

    return walk(brief).strip() if brief else ""


def brief_anomaly(brief):
    """Why a brief is anomalous, or None when it looks fine: text that was
    never meant as a brief (a description paragraph, a banner) picked up as
    one."""
    if len(brief) < 4:
        return "too short"
    if len(brief) > 160:
        return f"too long, reads like a description ({len(brief)} chars)"
    run = re.search(r"([^\w\s])(\s*\1){3,}", brief)
    if run:
        return f"run of repeated '{run.group(1)}'"
    visible = [c for c in brief if not c.isspace()]
    special = sum(1 for c in visible if not c.isalnum())
    if special > len(visible) / 2:
        return "mostly punctuation"
    return None


# -------- phase 3: fix until mrdocs reports no warnings

def phase_warnings():
    previous = None
    for round in range(1, MAX_ROUNDS + 1):
        code, out, cmd = run_mrdocs(
            ["--generator=noop", *STRICT, "--log-level=warn"])
        blocks = diagnostics(out)
        if code == 0 and not blocks and "Extracted 0 declarations" not in out:
            print(f"[warnings] clean after {round - 1} round(s).")
            return True
        headers = tuple(b.splitlines()[0] for b in blocks)
        if headers and headers == previous:
            print("[warnings] the same locations came back unchanged; "
                  "stopping so a human can look.", file=sys.stderr)
            return False
        previous = headers

        batches = [blocks[i:i + BATCH] for i in range(0, len(blocks), BATCH)] \
            or [[out.strip() or "(no output)"]]
        print(f"[warnings] round {round}: {len(blocks)} location(s) in "
              f"{len(batches)} batch(es).")
        failures = 0
        for i, batch in enumerate(batches, 1):
            seconds, agent_code = run_agent(render(
                "warnings",
                command=shlex.join(cmd),
                batch_index=i,
                batch_count=len(batches),
                report="\n\n".join(batch)))
            note = " FAILED" if agent_code != 0 else ""
            failures += agent_code != 0
            print(f"[warnings] batch {i}/{len(batches)}{note} done in "
                  f"{seconds:.0f}s.")
        if failures == len(batches):
            agent_broken("warnings")
            return False
    print("[warnings] out of rounds.", file=sys.stderr)
    return False


def diagnostics(report):
    """Split the report into its location-grouped blocks."""
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
    return [b for b in blocks if b.strip()]


if __name__ == "__main__":
    sys.exit(main())
