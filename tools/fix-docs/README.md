# fix-docs

Drives an AI agent to bring a project's documentation up to date.

> The didactic version of this idea lives in `examples/generators/noop/fix-loop`
and stays small on purpose. This tool is the one that grows features.

## Phases

### 1. Compile

Extraction runs the `json` generator with warnings off and no error cap. While translation units fail to compile, the agent gets the compiler errors and fixes the config and shims until the whole project extracts. The corpus from the successful run is handed to the document phase, so a clean project is never extracted twice.

### 2. Document

The work-list is derived from that JSON file. It covers every problem the strict check reports that the corpus already carries, so nothing here needs another Mr.Docs run: 

- undocumented symbols, 
- missing briefs, 
- anomalous briefs (description paragraphs or banners picked up as briefs), 
- function and macro parameters with no `@param`, 
- `@param` entries that name no parameter, 
- functions that return a value with no `@return`, 
- unnamed parameters (the agent names them so they can be documented), 
- undocumented namespaces, and namespaces whose comments are attached at more than one reopening, and 
- undocumented enum values. 

Members copied from a base class are skipped, as the strict check skips them: their comment belongs to the original. Each task is reported at the declaration that carries the comment, not at the definition, and a symbol documented at several declarations gets one task per declaration with a pointer to the others, because Mr.Docs merges those comments field by field and the first one wins.

Namespaces get special treatment because they are reopened in every file. A stray `///` line above one `namespace X {` becomes the namespace's brief for the whole project, and documenting some other reopening does not displace it. So every documented reopening gets a task: the lowest (path, line) one is canonical and keeps the one-sentence brief, the others must move their comment onto the declaration it describes or delete it.
 
Tasks are ordered the way the index is: global namespace first (macros included), then one namespace level, then two, and alphabetically within a level. Files are visited in that order, so an interrupted run still leaves everything a reader sees first fully documented.
 
The driver groups tasks by file and hands the agent one file's complete list per call, so line drift stays inside a single visit and it can print progress and an ETA. Within a file the tasks are listed bottom to top (highest line number first), so each edit the agent makes never shifts the line numbers of the symbols it has not reached yet. 

Mr.Docs re-runs only between cycles, not between files. On a large tree that re-extraction takes as long as the initial one (about 30 minutes for LLVM), and the driver says so when it starts it. Progress is checkpointed to the state file, so an interrupted run resumes where it stopped.

Cycles continue while they change something, up to `--max-rounds`. The driver hashes a file before and after the agent's visit. A visit that leaves the file unchanged means the agent read the tasks and judged the symbols already documented, so the cause is on the Mr.Docs side and another visit will say the same: those tasks are parked at once, together with the agent's closing line, which the prompt asks it to make a one-line summary. As a backstop, a task the agent has edited `--stuck-after` times (default 3) that still comes back is parked too. Parked tasks leave the queue, the remaining cycles go to tasks that can move, and when nothing but parked tasks is left the phase ends. A cycle that brings back the exact same task list also ends it. Either way the tasks that survived are written next to the state file (`<state>.residual.md`, grouped by reason, each with what the agent said about it) and the run moves on to the warnings phase.

### 3. Warnings
 
A strict check runs once and every diagnostic block is collected. Every warning is treated as an error. An inner loop feeds them to the agent batch by batch. Mr.Docs re-runs only when the collected list is exhausted. The outer loop stops when Mr.Docs finds no documentation errors.

After the document phase, this is a backstop: it catches what needs extraction to see (documentation parse errors, broken references) and anything the document phase got wrong.

Two shapes of output are collected. The strict check groups its findings under a `path:line:col:` header. The doc-comment parser prints free-form lines such as `warning: HTML <u> tag not followed by end tag at path (line)`, once per translation unit that includes the header; these are folded into the same grouped shape and deduplicated. A warning with no location is dropped, since nobody can act on it.

Rounds are compared on what was said and where, with line numbers removed, against every earlier round. Edits shift lines, and an agent can oscillate between two fixes (add the `@param`, remove the `@param`), so a round that matches any earlier one means the agent is not converging and the phase stops with the offending locations listed.

## Usage

```bash
cd path/to/project
MRDOCS=/path/to/mrdocs AGENT="claude -p" \
python3 /path/to/mrdocs/tools/fix-docs/fix-docs.py docs/mrdocs.yml \
  --addons=... --stdlib-includes=... --libc-includes=... --clang-resource-dir=...
```

The first positional argument is the config. Everything the tool does not
recognize is forwarded to every mrdocs invocation (any `--max-errors` is
stripped), which is how the include and addon flags above reach mrdocs.

The same run with flags instead of environment variables:

```bash
cd path/to/project
python3 /path/to/mrdocs/tools/fix-docs/fix-docs.py docs/mrdocs.yml \
  --mrdocs /path/to/mrdocs --agent "claude -p" --parallel 8 \
  --addons=... --stdlib-includes=... --libc-includes=... --clang-resource-dir=...
```

Each option can be a command-line flag or an environment variable. A flag
wins over its variable, and the variable wins over the default. Run with
`--help` for the full list.

- `--mrdocs` (env `MRDOCS`): the mrdocs binary (default: `mrdocs` on PATH).
- `--agent` (env `AGENT`): the agent's non-interactive command, run with the
  rendered prompt as its final argument (default `agent -p --trust`, or
  `claude -p` for Claude Code, `codex exec` for Codex). Cursor's agent runs
  safe tool calls on its own in print mode and blocks dangerous ones, so it
  needs no `-f`; `--trust` only skips the workspace-trust prompt.
- `--parallel` (env `PARALLEL`): concurrent agents in the document phase
  (default 1). File batches are independent, so this scales until the machine
  runs out of cores or the agent runs out of rate limit.
- `--batch` (env `BATCH`): diagnostic blocks per agent call in the warnings
  phase (default 30).
- `--max-rounds` (env `MAX_ROUNDS`): how many times mrdocs may re-run in each
  phase (default 40). It never limits the fan-out between runs: in the
  document phase every file in the work-list gets its agent visit. Each phase
  also stops early when a round changes nothing (the same task list or the
  same warning locations come back).
- `--stuck-after` (env `STUCK_AFTER`): backstop for tasks the agent keeps
  editing without clearing them; after this many such visits (default 3)
  they are parked. A visit that leaves the file unchanged parks its tasks
  at once. Both records live in the state file, so they carry across
  interrupted runs.
- `--retries` / `--retry-pause` (env `RETRIES`, `RETRY_PAUSE`): a failed agent
  call (non-zero exit: a rate limit, a dropped connection) is retried this
  many times (default 3), pausing `--retry-pause` seconds before the first
  retry (default 60) and doubling after each. Only a call that fails every
  attempt counts as failed; a cycle where every call fails stops the run,
  with the cycle's tasks written to the residual file and the checkpoint
  intact so a later run resumes there.
- `--state` (env `STATE`): checkpoint path. The default is under the system
  temp directory, keyed by the config's absolute path, so it survives an
  interrupted run without ever landing in the project tree. The driver's own
  lines are also appended to `<state>.log`, and a summary of the three phases
  is printed last, because the agents' output scrolls the driver's lines off
  the terminal. The exit code is non-zero when any phase did not end clean.

Prompts live in `prompts/` and are the place to teach the agent new repair
strategies.

## The work-list

The `json` generator writes `reference.json`, and the work-list is derived
from it in the driver. The rules that decide what needs work live in
`fix-docs.py`: `classify` turns each symbol into its tasks (skip non-regular
extraction and inherited copies, require a location), `function_reasons`
and `macro_reasons` mirror the strict parameter and return checks,
`namespace_tasks` handles reopenings, and `brief_anomaly` flags a brief that
was never meant as one (too short, too long, mostly punctuation, a run of
repeated punctuation). To change or add a rule, edit those functions.

The dump reflects the same config and flags as every other phase, so nothing
is copied or patched into the target project. The compile phase already runs
the `json` generator, so its corpus is reused for the first document cycle.
The warnings phase runs `--generator=noop`.
