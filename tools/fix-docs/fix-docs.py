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
--stuck-after, --retries, --retry-pause, --state) each fall back to an
environment variable (MRDOCS, AGENT, PARALLEL, BATCH, MAX_ROUNDS, STUCK_AFTER,
RETRIES, RETRY_PAUSE, STATE) then a default. Run with --help for the full list.
The cap bounds mrdocs re-runs in every phase, never the per-file or per-batch
fan-out between runs, and each phase also stops early when a full round
changes nothing.
"""

from __future__ import annotations

import argparse
import hashlib
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
    p.add_argument("--agent", default=env("AGENT", "agent -p --trust"),
                   help="agent command; the prompt is appended as its last "
                        "argument (env AGENT; default: 'agent -p --trust')")
    p.add_argument("--parallel", type=int, default=int(env("PARALLEL", "1")),
                   help="concurrent agents in the document phase "
                        "(env PARALLEL; default: 1)")
    p.add_argument("--batch", type=int, default=int(env("BATCH", "30")),
                   help="diagnostics per agent call in the warnings phase "
                        "(env BATCH; default: 30)")
    p.add_argument("--max-rounds", type=int,
                   default=int(env("MAX_ROUNDS", "40")),
                   help="mrdocs re-run cap in every phase; a phase also stops "
                        "early when a round changes nothing "
                        "(env MAX_ROUNDS; default: 40)")
    p.add_argument("--stuck-after", type=int,
                   default=int(env("STUCK_AFTER", "3")),
                   help="backstop: visits in which the agent edited the file "
                        "yet the task came back, before it is parked; a visit "
                        "that leaves the file unchanged parks it at once "
                        "(env STUCK_AFTER; default: 3)")
    p.add_argument("--retries", type=int, default=int(env("RETRIES", "3")),
                   help="how many times a failed agent call is retried, with "
                        "a pause doubling from --retry-pause "
                        "(env RETRIES; default: 3)")
    p.add_argument("--retry-pause", type=int,
                   default=int(env("RETRY_PAUSE", "60")),
                   help="seconds before the first retry of a failed agent "
                        "call (env RETRY_PAUSE; default: 60)")
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
RETRIES = ARGS.retries
STUCK_AFTER = ARGS.stuck_after
RETRY_PAUSE = ARGS.retry_pause
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
    log(f"config={CONFIG} agent={shlex.join(AGENT)} parallel={PARALLEL}")
    log(f"driver log: {LOG_PATH}")
    corpus = phase_compile()
    if corpus is None:
        return summary(compile=False)
    documented = phase_document(corpus)
    if documented is None:
        return summary(document="broken")
    # A stalled document phase is not a reason to skip the warnings phase: it
    # is the backstop that sees what the corpus dump cannot (parse errors,
    # broken references), and its own stop rule (same locations twice) keeps
    # it from spinning.
    clean = phase_warnings()
    return summary(document=documented, warnings=clean)


def summary(compile=True, document=None, warnings=None):
    """Print the outcome in one place, last, so it is the first thing seen
    when scrolling back through the agents' output. Non-zero when any phase
    did not finish clean."""
    ok = compile and document is True and warnings is True
    log("")
    log("==================== fix-docs summary ====================")
    log(f"compile:  {'clean' if compile else 'FAILED (see above)'}")
    if document is None:
        log("document: not run")
    elif document == "broken":
        log("document: every agent invocation FAILED; nothing documented")
    else:
        log("document: clean" if document else
            f"document: NOT converged; residual work-list in {RESIDUAL_PATH}")
    if warnings is None:
        log("warnings: not run")
    else:
        log("warnings: clean" if warnings else "warnings: NOT clean (see above)")
    log(f"driver log: {LOG_PATH}")
    log("==========================================================")
    return 0 if ok else 1


# -------- shared helpers

LOG_PATH = STATE_PATH.with_suffix(".log")
RESIDUAL_PATH = STATE_PATH.with_suffix(".residual.md")
LOG_LOCK = Lock()


def log(message, err=False):
    """Print one driver line and append it to the log file. The agents write
    straight to the terminal and drown the driver's lines, so the log file
    is the place to read what the driver decided."""
    print(message, file=sys.stderr if err else sys.stdout, flush=True)
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with LOG_LOCK:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(f"{stamp} {message}\n")


def run_mrdocs(args):
    """Run mrdocs; return (returncode, ansi-stripped combined output)."""
    cmd = [MRDOCS, f"--config={CONFIG}", *EXTRA, *args]
    run = subprocess.run(cmd, capture_output=True, text=True)
    return run.returncode, ANSI.sub("", run.stdout + run.stderr), cmd


def run_agent(prompt):
    """Hand one rendered prompt to the agent; return (seconds, exit code,
    output). The output is streamed to the terminal as it arrives and also
    kept, because the prompts ask the agent to end with a one-line summary
    and that line is the best explanation of a task it declined to change.
    A non-zero exit means the agent invocation itself failed (crash, bad
    command, rate-limit abort), not that it ran but left work undone. Such
    failures are usually transient (a rate limit, a dropped connection), so
    the call is retried with a growing pause before it counts as failed."""
    start = time.monotonic()
    for attempt in range(1, RETRIES + 2):
        proc = subprocess.Popen(
            AGENT + [prompt], stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, errors="replace")
        lines = []
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            lines.append(line)
        code = proc.wait()
        if code == 0 or attempt > RETRIES:
            break
        pause = RETRY_PAUSE * 2 ** (attempt - 1)
        log(f"[agent] exit {code}; retry {attempt}/{RETRIES} in {pause}s.",
            err=True)
        time.sleep(pause)
    return time.monotonic() - start, code, "".join(lines)


def last_line(output):
    """The agent's closing summary: its last non-empty output line."""
    for line in reversed(output.splitlines()):
        if line.strip():
            return line.strip()[:300]
    return "(no output)"


def file_digest(path):
    try:
        return hashlib.sha1(Path(path).read_bytes()).hexdigest()
    except OSError:
        return None


# Printed when every agent in a phase failed to run. The work is real, so the
# problem is the agent command, not the project: stop before the next phase so
# the user can point --agent (or AGENT) at another tool.
def agent_broken(phase):
    log(f"[{phase}] every agent invocation failed (non-zero exit). The "
        f"agent command looks broken or unavailable; stopping so you can "
        f"switch agents (--agent / AGENT). Later phases will not run.",
        err=True)


def render(name, **values):
    text = (HERE / "prompts" / f"{name}.md").read_text(encoding="utf-8")
    for key, value in values.items():
        text = text.replace("{" + key + "}", str(value))
    return text


def load_state():
    state = {"documented": [], "seen": {}, "parked": {}}
    if STATE_PATH.exists():
        state.update(json.loads(STATE_PATH.read_text(encoding="utf-8")))
    return state


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
            log(f"[compile] clean after {round - 1} repair round(s).")
            return corpus
        if not errors:
            errors = [out.strip()[-4000:] or "(no output)"]
        log(f"[compile] round {round}: {len(errors)} error line(s); "
              f"handing them to the agent.")
        seconds, agent_code, _ = run_agent(render(
            "compile",
            command=shlex.join(cmd),
            config=CONFIG,
            errors="\n".join(errors[:400])))
        if agent_code != 0:
            agent_broken("compile")
            return None
        log(f"[compile] agent took {seconds:.0f}s.")
    log("[compile] out of rounds.", err=True)
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
    previous = None
    for cycle in range(1, MAX_ROUNDS + 1):
        # The compile phase already extracted; reuse its corpus for cycle 1
        # instead of extracting again just to say the same thing.
        if cycle == 1 and initial_corpus is not None:
            tasks = work_list_from(initial_corpus)
        else:
            log("[document] re-extracting with mrdocs to build the next "
                "work-list; on a large tree this takes as long as the "
                "initial extraction.")
            started_dump = time.monotonic()
            tasks = dump_work_list()
            log(f"[document] extraction took "
                f"{(time.monotonic() - started_dump) / 60:.0f}min.")

        # Two kinds of task are not worth another visit. One the agent has
        # already looked at and left the file untouched: it judged the
        # symbol documented, so the cause is on the mrdocs side, and its
        # closing line says what it saw. And, as a backstop, one the agent
        # has edited STUCK_AFTER times that still comes back. Both are
        # parked, so the remaining cycles (and their long extractions) go
        # to tasks that can move.
        seen = state["seen"]
        declined = state["parked"]
        for t in tasks:
            t["cycles"] = seen.get(stall_id(t), 0)
            t["agent"] = declined.get(stall_id(t))
        parked = [t for t in tasks
                  if t["agent"] or t["cycles"] >= STUCK_AFTER]
        active = [t for t in tasks
                  if not t["agent"] and t["cycles"] < STUCK_AFTER]
        for t in active:
            seen[stall_id(t)] = t["cycles"] + 1
        save_state(state)

        done = set(map(tuple, state["documented"]))
        by_file = {}
        for t in active:
            key = (t["file"], t["name"], t["reason"])
            if key not in done:
                by_file.setdefault(t["file"], []).append(t)
        if not by_file:
            if parked:
                write_residual(parked)
                log(f"[document] every remaining task ({len(parked)}) is "
                    f"parked: the agent looked and left the file unchanged, "
                    f"or edited it {STUCK_AFTER} times without clearing the "
                    f"task. Listed by reason, with what the agent said, in "
                    f"{RESIDUAL_PATH}. Moving on to the warnings phase.",
                    err=True)
                return False
            log(f"[document] work-list clean after cycle {cycle - 1}.")
            return True
        if parked:
            log(f"[document] {len(parked)} task(s) parked (agent declined or "
                f"{STUCK_AFTER} visits without effect); {len(active)} active.")
        # Progress is measured on the set of tasks, not its size: documenting
        # a symbol can legitimately surface new detail tasks (its parameters,
        # its return value), so a longer list can still be progress. Only an
        # identical list means the agent is not converging.
        current = frozenset(stall_key(t) for ts in by_file.values() for t in ts)
        if current == previous:
            write_residual(parked + active)
            log(f"[document] cycle {cycle - 1} changed nothing: the same "
                f"{len(current)} task(s) came back. Listed by reason in "
                f"{RESIDUAL_PATH}. Moving on to the warnings phase.",
                err=True)
            return False
        previous = current

        # Do the files nearest the top of the index first, so an interrupted
        # run still leaves everything a reader sees first fully documented. A
        # file's priority is the best index rank among its tasks; the tasks
        # are already sorted by rank, so the first one is that best.
        ordered_files = sorted(
            by_file.items(), key=lambda kv: index_rank(kv[1][0]))
        total_files = len(by_file)
        total_tasks = sum(len(v) for v in by_file.values())
        log(f"[document] cycle {cycle}: {total_tasks} task(s) across "
              f"{total_files} file(s).")

        lock = Lock()
        finished = [0]
        failed = [0]
        declined_files = [0]
        started = time.monotonic()

        def fix_file(file, file_tasks):
            file_tasks.sort(key=lambda t: -t["line"])
            listing = "\n".join(
                f"- line ~{t['line']}: {t['kind']} `{t['name']}`: {t['reason']}"
                + (f"\n  current brief: \"{t['brief']}\"" if t.get("brief") else "")
                for t in file_tasks)
            before = file_digest(file)
            seconds, code, output = run_agent(
                render("document", file=file, tasks=listing))
            unchanged = code == 0 and file_digest(file) == before
            with lock:
                finished[0] += 1
                if unchanged:
                    # The agent read the tasks and decided there was nothing
                    # to change. Another visit will say the same, so park
                    # them now with its explanation instead of paying for a
                    # re-extraction to find out.
                    for t in file_tasks:
                        state["parked"][stall_id(t)] = last_line(output)
                    declined_files[0] += 1
                    save_state(state)
                elif code == 0:
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
                note = " FAILED" if code != 0 else \
                    " UNCHANGED (agent declined; tasks parked)" if unchanged \
                    else ""
                log(f"[document] {finished[0]}/{total_files} files{note} "
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
            write_residual(tasks)
            agent_broken("document")
            log(f"[document] the {len(tasks)} task(s) of this cycle are listed "
                f"in {RESIDUAL_PATH}; the checkpoint keeps every earlier "
                f"cycle's progress, so re-running resumes here.", err=True)
            return None
        if failed[0]:
            log(f"[document] {failed[0]}/{total_files} file(s) had a "
                  f"failing agent; they will be retried next cycle.")
        if declined_files[0]:
            log(f"[document] {declined_files[0]}/{total_files} file(s) came "
                f"back unchanged; their tasks are parked with the agent's "
                f"explanation.")

        # The checkpoint only protects a cycle against interruption. Once a
        # cycle completes, the next dump is the ground truth: a task that
        # still shows up was not really fixed and must be queued again.
        state["documented"] = []
        save_state(state)
    write_residual(tasks)
    log(f"[document] out of rounds after {MAX_ROUNDS} cycles: {len(tasks)} "
        f"task(s) left, listed by reason in {RESIDUAL_PATH}. Moving on to "
        f"the warnings phase.", err=True)
    return False


def stall_id(task):
    """stall_key as one string, usable as a JSON object key."""
    return "\x1f".join(stall_key(task))


def stall_key(task):
    """What identifies a task across cycles: its file, symbol, and reason
    with every line number removed. Edits shift lines, and the reason can
    quote other locations, so a task that survives unchanged must still
    compare equal to itself."""
    return (task["file"], task["name"], re.sub(r":\d+", "", task["reason"]))


def write_residual(tasks):
    """Dump the tasks that survived every cycle, grouped by reason, so a
    human can see what the agent kept failing at without re-running."""
    by_reason = {}
    for t in tasks:
        by_reason.setdefault(t["reason"].split(":")[0], []).append(t)
    lines = [f"# Residual work-list ({len(tasks)} task(s))", ""]
    for reason, group in sorted(by_reason.items(), key=lambda kv: -len(kv[1])):
        lines += [f"## {reason} ({len(group)})", ""]
        lines += [f"- {t['file']}:{t['line']} {t['kind']} `{t['name']}`: "
                  f"{t['reason']}" + (f" (brief: \"{t['brief']}\")"
                                     if t.get("brief") else "")
                  + (f"\n  agent said: {t['agent']}" if t.get("agent") else
                     f" [edited {t['cycles']} time(s) without effect]"
                     if t.get("cycles") else "")
                  for t in group]
        lines.append("")
    RESIDUAL_PATH.parent.mkdir(parents=True, exist_ok=True)
    RESIDUAL_PATH.write_text("\n".join(lines), encoding="utf-8")


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
    """Derive the work-list from a corpus, in index order, one task per
    (file, line, symbol, reason). Overloads that share a line, or the same
    symbol reached through several ids, would otherwise hand the agent the
    same task many times over and bury the rest of the file's list."""
    symbols = corpus.get("symbols", [])
    by_id = {s["id"]: s for s in symbols if "id" in s}
    shared = shared_comment_lines(symbols)
    seen = set()
    tasks = []
    for s in symbols:
        for t in tasks_for(s, by_id, shared):
            key = (t["file"], t["line"], t["name"], t["reason"])
            if key not in seen:
                seen.add(key)
                tasks.append(t)
    tasks.sort(key=index_rank)
    return tasks


def index_rank(task):
    """Sort key placing a task where its symbol sits in the index: shallower
    namespaces first (the global namespace, then one level, then two...),
    alphabetical within a level."""
    return (task["depth"], task["name"].lower())


def tasks_for(sym, by_id, shared=frozenset()):
    """Every work-list entry a symbol needs (zero or more).

    These mirror the subset of the strict warnings that the corpus dump
    already carries, so
    the warnings phase has almost nothing left to catch: undocumented
    symbols (namespaces included), missing or anomalous briefs, function
    parameters with no doc, and undocumented enum values. Only regular
    (documented-surface) symbols are considered. The global namespace, which
    has no name and no location, drops out on its own below.
    """
    if sym.get("extraction", "regular") != "regular":
        return []
    # A member copied from a base class carries the base's documentation;
    # the strict check skips these too. The task belongs to the original.
    if sym.get("isCopyFromInherited"):
        return []
    kind = sym.get("kind", "?")
    name = qualified_name(sym, by_id)
    tasks = classify(sym, by_id, kind, name, shared)
    # Every task from one symbol shares its index depth (the enum-constant's
    # namespaces are the same as its enum's). Stamp it once for sorting.
    depth = namespace_depth(sym, by_id)
    for task in tasks:
        task["depth"] = depth
    return tasks


def classify(sym, by_id, kind, name, shared=frozenset()):
    """The reasons a symbol needs work, as unranked tasks."""
    # Enum values are separate symbols with no location of their own, so
    # report them at the parent enum. Only when the enum itself is
    # documented: a wholly-undocumented enum stays one task, and its values
    # get written when the agent documents it.
    if kind == "enum-constant":
        parent = by_id.get(sym.get("parent"))
        if sym.get("doc") or not (parent and parent.get("doc")):
            return []
        locs = documented_locs(parent) or as_list(symbol_loc(parent))
        return [make_task(locs[0], name, kind, "undocumented enum value")] \
            if locs else []

    if kind == "namespace":
        return namespace_tasks(sym, name)

    # A specialization Mr.Docs instantiated on its own (template arguments,
    # no template parameters of its own) is documented by its primary
    # template; there is no declaration of it to put a comment on. When the
    # primary is documented, whatever the specialization symbol lacks is
    # Mr.Docs failing to carry the doc over, not missing documentation.
    template = sym.get("template") or {}
    if template.get("args") and not template.get("params"):
        primary = by_id.get(template.get("primary"))
        if primary and primary.get("doc"):
            return []

    if not sym.get("doc"):
        # The whole symbol is undocumented; the agent writes its brief,
        # params, and enum values in one comment, so no detail tasks here.
        loc = symbol_loc(sym)
        return [make_task(loc, name, kind, "undocumented")] if loc else []

    reasons = []
    brief = brief_text(sym)
    if not brief:
        reasons.append(("no-brief", None))
    else:
        anomaly = brief_anomaly(brief)
        if anomaly:
            reasons.append((f"anomalous-brief: {anomaly}", brief))

    if kind == "function":
        reasons += function_reasons(sym, shared)
    elif kind == "macro":
        reasons += macro_reasons(sym)
    if not reasons:
        return []

    # The comment lives on a documented declaration, not necessarily on the
    # definition, so that is where the agent must go. When several
    # declarations carry a comment, Mr.Docs merges them field by field and
    # the first one wins, so each of them gets the task and a pointer to
    # the others: fixing only one leaves the stale text free to win again.
    locs = documented_locs(sym) or as_list(symbol_loc(sym))
    tasks = []
    for loc in locs:
        others = [f"{l['sourcePath']}:{l['lineNumber']}" for l in locs
                  if l is not loc]
        note = ""
        if others:
            note = ("; also documented at " + ", ".join(others) +
                    " (Mr.Docs merges every declaration's comment, first "
                    "one wins: fix the copy here too)")
        for reason, text in reasons:
            tasks.append(make_task(loc, name, kind, reason + note, text))
    return tasks


def function_reasons(sym, shared=frozenset()):
    """What the strict check would say about a documented function's
    parameters and return value, read off the corpus: named parameters with no
    @param, @param entries naming no parameter, unnamed parameters (which
    must first be given a name before they can be documented at all), and a
    non-void return with no @return. Deleted functions are exempt from the
    parameter and return checks, as in Mr.Docs.

    Parameter checks are skipped for a function that shares its line, and
    so its one comment, with a function of a different signature: a macro
    that expands to a static and a member overload, say. No single comment
    can name the parameters of both, so the agent would only flip an @param
    on and off between cycles."""
    reasons = []
    params = sym.get("params") or []
    named = [p["name"] for p in params if p.get("name")]
    documented = doc_param_names(sym)
    loc = symbol_loc(sym)
    shares_comment = loc and (loc["sourcePath"], int(loc["lineNumber"])) in shared
    if not sym.get("isDeleted"):
        missing = [n for n in named if n not in documented]
        if missing and not shares_comment:
            reasons.append(("missing param doc: " + ", ".join(missing), None))
        if missing_return_doc(sym):
            reasons.append(("missing return doc: the function returns a "
                            "value and the comment has no @return", None))
    bogus = [n for n in documented if n not in named]
    if bogus and shares_comment:
        bogus = []
    if bogus:
        reasons.append(("documented parameter(s) that do not exist: "
                        + ", ".join(bogus) +
                        " (rename to the real parameter or remove)", None))
    unnamed = sum(1 for p in params if not p.get("name"))
    if unnamed:
        reasons.append((f"unnamed parameter(s): {unnamed}", None))
    return reasons


def macro_reasons(sym):
    """The parameter checks for a documented function-like macro. Only the
    named parameters must be documented; the variadic list is optional and
    may be documented as `...` or `__VA_ARGS__`."""
    params = sym.get("parameters") or []
    documented = doc_param_names(sym)
    variadic = {"...", "__VA_ARGS__"} if sym.get("isVariadic") else set()
    reasons = []
    missing = [n for n in params if n not in documented]
    if missing:
        reasons.append(("missing param doc: " + ", ".join(missing), None))
    bogus = [n for n in documented if n not in params and n not in variadic]
    if bogus:
        reasons.append(("documented parameter(s) that do not exist: "
                        + ", ".join(bogus) +
                        " (rename to the real parameter or remove)", None))
    return reasons


def shared_comment_lines(symbols):
    """The (file, line) pairs where more than one function with a different
    parameter list is declared: the product of a macro expanding to several
    declarations, which all receive the comment written above the macro."""
    signatures = {}
    for s in symbols:
        if s.get("kind") != "function" or s.get("isCopyFromInherited"):
            continue
        loc = symbol_loc(s)
        if not loc:
            continue
        key = (loc["sourcePath"], int(loc["lineNumber"]))
        sig = tuple(p.get("name") or "" for p in s.get("params") or [])
        signatures.setdefault(key, set()).add(sig)
    return frozenset(k for k, sigs in signatures.items() if len(sigs) > 1)


def doc_param_names(sym):
    """The names given to @param entries in a symbol's comment."""
    return [p.get("name") for p in (sym.get("doc") or {}).get("params", [])
            if p.get("name")]


def missing_return_doc(sym):
    """A function whose return type is not void and whose comment has no
    @return. Constructors and destructors have a void return type in the
    corpus, so they drop out here like they do in the strict check."""
    if (sym.get("doc") or {}).get("returns"):
        return False
    rt = sym.get("returnType")
    if not rt:
        return False
    if rt.get("kind") == "named" and \
            (rt.get("name") or {}).get("identifier") == "void":
        return False
    return True


def documented_locs(sym):
    """The declarations of a symbol that carry a comment, in a stable order,
    without duplicates."""
    seen = set()
    out = []
    for l in sorted(all_locs(sym),
                    key=lambda l: (l["sourcePath"], int(l["lineNumber"]))):
        key = (l["sourcePath"], int(l["lineNumber"]))
        if l.get("documented") and key not in seen:
            seen.add(key)
            out.append(l)
    return out


def as_list(loc):
    return [loc] if loc else []


def namespace_tasks(sym, name):
    """Tasks for a namespace, which is reopened in many files.

    Mr.Docs merges the comment attached to every reopening, field by field,
    first one wins. So the brief can come from any file that happens to have
    a comment right above `namespace X {`, often a banner or a description of
    the declaration that follows. A task at the definition location alone
    cannot fix that: the agent documents that reopening and the stray comment
    elsewhere keeps winning. So every documented reopening gets a task. The
    lowest (path, line) documented reopening is the canonical one and keeps
    the brief; the others must move their comment onto the declaration it
    describes or delete it. Once the namespace is documented at exactly one
    place with a sane brief, no task remains.
    """
    kind = "namespace"
    documented = documented_locs(sym)
    if not sym.get("doc") or not documented:
        loc = symbol_loc(sym)
        return [make_task(loc, name, kind, "undocumented namespace")] \
            if loc else []

    brief = brief_text(sym)
    anomaly = brief_anomaly(brief) if brief else None
    if brief and not anomaly and len(documented) == 1:
        return []

    if not brief:
        what = "namespace has no brief"
    elif anomaly:
        what = f"anomalous namespace brief: {anomaly}"
    else:
        what = "namespace brief is fine, but the doc is split"
    others = [f"{l['sourcePath']}:{l['lineNumber']}" for l in documented]
    tasks = []
    for i, loc in enumerate(documented):
        if i == 0:
            role = "canonical reopening: the one-sentence brief for the " \
                   "namespace as a whole lives here"
        else:
            role = f"non-canonical reopening: move the comment attached " \
                   f"here onto the declaration it describes, or delete it; " \
                   f"the brief lives at {others[0]}"
        reason = f"{what}; comments are attached at {len(documented)} " \
                 f"reopening(s) ({', '.join(others)}); {role}"
        tasks.append(make_task(loc, name, kind, reason,
                               brief if i == 0 else None))
    return tasks


def all_locs(sym):
    """Every usable location a symbol has: the definition and every
    declaration."""
    info = sym.get("loc") or {}
    locs = list(info.get("loc") or [])
    if info.get("defLoc"):
        locs.append(info["defLoc"])
    return [l for l in locs
            if l.get("sourcePath") and l.get("lineNumber")]


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
    seen = []
    for round in range(1, MAX_ROUNDS + 1):
        code, out, cmd = run_mrdocs(
            ["--generator=noop", *STRICT, "--log-level=warn"])
        blocks = diagnostics(out)
        if code == 0 and not blocks and "Extracted 0 declarations" not in out:
            log(f"[warnings] clean after {round - 1} round(s).")
            return True
        # Compare rounds on what was said and where, minus line numbers:
        # edits shift lines, so the same complaint at a moved line is still
        # the same complaint. Every past round is kept, not just the last,
        # because an agent can oscillate between two fixes (add the @param,
        # remove the @param) and each round then differs from its
        # predecessor while nothing converges.
        signature = frozenset(re.sub(r":\d+", "", b) for b in blocks)
        if signature in seen:
            log(f"[warnings] round {round} came back identical to round "
                f"{seen.index(signature) + 1} (ignoring line numbers); the "
                f"agent is not converging on these {len(blocks)} location(s). "
                f"Stopping so a human can look:", err=True)
            for b in blocks:
                log("    " + b.splitlines()[0].rstrip(":"), err=True)
            return False
        seen.append(signature)

        batches = [blocks[i:i + BATCH] for i in range(0, len(blocks), BATCH)] \
            or [[out.strip() or "(no output)"]]
        log(f"[warnings] round {round}: {len(blocks)} location(s) in "
              f"{len(batches)} batch(es).")
        failures = 0
        for i, batch in enumerate(batches, 1):
            seconds, agent_code, _ = run_agent(render(
                "warnings",
                command=shlex.join(cmd),
                batch_index=i,
                batch_count=len(batches),
                report="\n\n".join(batch)))
            note = " FAILED" if agent_code != 0 else ""
            failures += agent_code != 0
            log(f"[warnings] batch {i}/{len(batches)}{note} done in "
                  f"{seconds:.0f}s.")
        if failures == len(batches):
            agent_broken("warnings")
            return False
    log("[warnings] out of rounds.", err=True)
    return False


FREEFORM = re.compile(
    r"^(?:warning: )?(?P<message>.*?)(?: \(src/[^)]*\))? at (?P<path>\S+) "
    r"\((?P<line>\d+)\)\s*$")


def diagnostics(report):
    """Split the report into its location-grouped blocks.

    Two shapes are recognized. The strict check groups its findings under a
    `path:line:col:` header with numbered items and a source snippet. The
    doc-comment parser instead prints one free-form line per finding, such
    as `warning: HTML <u> tag not followed by end tag at path (line)`, and
    prints it again for every translation unit that includes the header.
    Those are rewritten into the grouped shape and deduplicated so the agent
    sees each of them once. A free-form warning with no location is dropped:
    nobody can act on it.
    """
    blocks, current, freeform = [], [], {}
    for line in report.splitlines():
        if FOOTER.match(line):
            continue
        if LOCATION.match(line):
            if current:
                blocks.append("\n".join(current))
            current = [line]
            continue
        if current and (line.startswith(" ") or not line.strip()):
            current.append(line)
            continue
        if current:
            blocks.append("\n".join(current))
            current = []
        m = FREEFORM.match(line)
        if m:
            key = (m["path"], int(m["line"]))
            freeform.setdefault(key, []).append(m["message"])
    if current:
        blocks.append("\n".join(current))
    for (path, line), messages in sorted(freeform.items()):
        items = "\n".join(f"    {i}) {msg}"
                          for i, msg in enumerate(dict.fromkeys(messages), 1))
        blocks.append(f"{path}:{line}:1:\n{items}")
    return [b for b in blocks if b.strip()]


if __name__ == "__main__":
    sys.exit(main())
