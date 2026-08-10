# AI repair loop (no-op generator)

This example closes the loop on the [documentation check](../documentation-check):
it runs the same strict `noop` check and, while the check fails, hands a **capped
batch** of diagnostics to an AI agent to repair, then runs again until extraction
is clean (or the same locations repeat).

`include/geo/area.hpp` leaves several parameters undocumented on purpose, so the
loop has something to fix.

## Run the loop

```bash
python fix-docs.py
```

The agent command comes from the `AGENT` environment variable and defaults to
`agent -p -f` (Cursor Agent, non-interactive). Point it at another tool the same
way, for example `AGENT="claude -p" python fix-docs.py` or
`AGENT="codex exec" python fix-docs.py`. Set `MRDOCS` to the mrdocs binary if it
is not on `PATH`.

The instructions given to the agent live in [`prompt.md`](./prompt.md) beside
this script. Edit that file to teach new repair strategies (shims, macro
expansions, and so on). The prompt explains `--max-errors`: a flat warning
*count* under the cap usually means the frontier moved, not that the loop
stalled.

`fix-docs.py` also forces the strict warning flags on the CLI so the loop does
not depend on `mrdocs.yml` remembering them (generation configs often turn them
off on purpose).

There is no CTest target here: the loop needs a real agent to make progress, so
it cannot run unattended in CI.

See `generators/noop.adoc` in the documentation for the full write-up.
