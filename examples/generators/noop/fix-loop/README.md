# AI repair loop (no-op generator)

The [documentation check](../documentation-check) in a loop. While the strict check fails, `fix-docs.py` hands a capped batch of diagnostics to an AI agent and runs again, until extraction is clean or the same locations repeat. The header leaves parameters undocumented for it to fix. There is no CTest target, since it needs a real agent.

```
python fix-docs.py
AGENT="claude -p" python fix-docs.py
```

The agent command comes from `AGENT` (default `agent -p -f`; `codex exec` works too). `MRDOCS` names the binary if it is not on `PATH`. The agent instructions are in [prompt.md](./prompt.md); edit them to teach new repair strategies, such as shims. The strict flags are in `mrdocs.yml`.

See `generators/noop.adoc` in the documentation. `tools/fix-docs` is the larger version of this loop.
