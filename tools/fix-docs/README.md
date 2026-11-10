# fix-docs

Drives an AI agent to bring a project's documentation up to date. `examples/generators/noop/fix-loop` is the small version of the same loop. This tool adds the features.

It runs three phases against the `mrdocs.yml` of a project:

1. Compile: the agent fixes the config and shims until the whole project extracts.
2. Document: a work-list derived from the corpus, one agent call per file, with progress saved so an interrupted run resumes.
3. Warnings: the diagnostics of the strict check go to the agent in batches.

Each phase stops when a round changes nothing. Tasks the agent cannot clear are parked and written to `<state>.residual.md`. The driver log is `<state>.log`, and a summary ends the run. The exit code is non-zero when a phase did not end clean.

```
cd path/to/project
MRDOCS=/path/to/mrdocs AGENT="claude -p" \
python3 /path/to/mrdocs/tools/fix-docs/fix-docs.py docs/mrdocs.yml [mrdocs flags]

  --mrdocs PATH       mrdocs binary (env MRDOCS, default: on PATH)
  --agent CMD         agent command (env AGENT, default: agent -p --trust)
                      claude -p for Claude Code, codex exec for Codex
                      --trust only skips the workspace-trust prompt of Cursor
  --parallel N        concurrent agents in the document phase (default 1)
  --batch N           diagnostics per agent call in the warnings phase (default 30)
  --max-rounds N      mrdocs re-runs per phase (default 40)
  --stuck-after N     edits before a task is parked (default 3)
  --retries N         retries of a failed agent call (default 3)
  --retry-pause SECS  pause before the first retry, doubling after (default 60)
  --state PATH        checkpoint file (default: under the temp directory)
```

A flag wins over its environment variable, and the variable over the default. Flags the tool does not recognize, such as `--addons`, go to every mrdocs invocation, except `--max-errors`. The agent instructions are in `prompts/`. The rules that decide what needs work are `classify`, `function_reasons`, `macro_reasons`, `namespace_tasks` and `brief_anomaly` in `fix-docs.py`.
