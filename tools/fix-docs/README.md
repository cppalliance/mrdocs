# fix-docs

Drives an AI agent to bring a project's documentation up to date.

> The didactic version of this idea lives in `examples/generators/noop/fix-loop`
and stays small on purpose. This tool is the one that grows features.

## Phases

### 1. Compile

Extraction runs the `json` generator with warnings off and no error cap. While translation units fail to compile, the agent gets the compiler errors and fixes the config and shims until the whole project extracts. The corpus from the successful run is handed to the document phase, so a clean project is never extracted twice.

### 2. Document

The work-list is derived from that JSON file. It covers every problem the strict check can find offline: 

- undocumented symbols, 
- missing briefs, 
- anomalous briefs (description paragraphs or banners picked up as briefs), 
- function parameters with no `@param`, 
- unnamed parameters (the agent names them so they can be documented), 
- undocumented namespaces, and 
- undocumented enum values. 
 
Tasks are ordered the way the index is: global namespace first (macros included), then one namespace level, then two, and alphabetically within a level. Files are visited in that order, so an interrupted run still leaves everything a reader sees first fully documented.
 
The driver groups tasks by file and hands the agent one file's complete list per call, so line drift stays inside a single visit and it can print progress and an ETA. Within a file the tasks are listed bottom to top (highest line number first), so each edit the agent makes never shifts the line numbers of the symbols it has not reached yet. 

Mr.Docs re-runs only between cycles, not between files. Progress is checkpointed to `fix-docs-state.json`, so an interrupted run resumes where it stopped.

### 3. Warnings
 
A strict check runs once and every diagnostic block is collected. Every warning is treated as an error. An inner loop feeds them to the agent batch by batch. Mr.Docs re-runs only when the collected list is exhausted. The outer loop stops when Mr.Docs finds no documentation errors.

After the document phase, this is a backstop: it catches what needs extraction to see (documentation parse errors, broken references) and anything the offline pass got wrong.

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
  rendered prompt as its final argument (default `agent -p -f`, or
  `claude -p` for Claude Code, `codex exec` for Codex).
- `--parallel` (env `PARALLEL`): concurrent agents in the document phase
  (default 1). File batches are independent, so this scales until the machine
  runs out of cores or the agent runs out of rate limit.
- `--batch` (env `BATCH`): diagnostic blocks per agent call in the warnings
  phase (default 30).
- `--max-rounds` (env `MAX_ROUNDS`): how many times mrdocs may re-run in the
  compile and warnings phases (default 40). It never limits the fan-out
  between runs: in the document phase every file in the work-list gets its
  agent visit.
- `--cycles` (env `CYCLES`): dump/fan-out passes in the document phase
  (default 3). A task that survives this many full passes means the agent is
  not converging.
- `--state` (env `STATE`): checkpoint path. The default is under the system
  temp directory, keyed by the config's absolute path, so it survives an
  interrupted run without ever landing in the project tree.

Prompts live in `prompts/` and are the place to teach the agent new repair
strategies.

## The work-list

The `json` generator writes `reference.json`, and the work-list is derived
from it in the driver. The rules that decide what needs work live in
`fix-docs.py`: `classify` turns each symbol into its tasks (skip non-regular
extraction, require a location), and `brief_anomaly` flags a brief that was
never meant as one (too short, too long, mostly punctuation, a run of
repeated punctuation). To change or add a rule, edit those functions.

The dump reflects the same config and flags as every other phase, so nothing
is copied or patched into the target project. The compile phase already runs
the `json` generator, so its corpus is reused for the first document cycle.
The warnings phase runs `--generator=noop`.
