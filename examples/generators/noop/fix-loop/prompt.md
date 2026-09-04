# Documentation repair batch

MrDocs ran a **strict documentation check** and produced the diagnostics at the
end of this prompt. Fix **every problem in this batch** so those specific
diagnostics go away on the next run.

## How this check was invoked

```text
{command}
```

Relevant knobs for this loop:

- `--generator=noop` — extract and lint docs; do not write reference pages.
- `--warn-as-error` — warnings fail the run (required for `--max-errors` to apply).
- `--max-errors={max_errors}` — stop after about this many documentation errors.
- Batch size for the agent: up to **{batch}** source location(s) from the report.

## This is only a batch

The check uses `--max-errors` (and the loop may further limit how many source
locations are handed to you). That cap exists so huge libraries stay fast: MrDocs
stops once it has enough errors to work on, and prints a truncation note.

Implications:

- The list below is **some** of the project's documentation problems, not all of
  them.
- Clearing this batch does **not** mean the project is clean.
- Seeing the same *count* of warnings next round usually means the frontier moved
  and a **new** set of diagnostics filled the cap — that is progress, not failure.
- Do **not** claim "MrDocs no longer reports any documentation warnings" for the
  whole project. Say only that the symbols in **this batch** are fixed (and
  re-check those symbols if you want confirmation).
- Do **not** re-run with `--max-errors=0` unless a human asked for a full audit.
  The cap is intentional.

## What to do

1. Read each diagnostic and the cited source.
2. Add real documentation comments (`/** … */` with a brief, and `@param` /
   `@return` where required). Describe what the symbol actually does — use the
   surrounding API, existing comments, and project man pages / docs when
   available. Do **not** invent behavior, and do **not** write placeholder briefs
   that only restate the identifier ("The FOO function", "FOO parameter").
3. If a parameter, struct or enum is **unnamed**, give it a sensible name so it can be
   documented; otherwise change comments only — no logic or API redesign.
4. Prefer documenting the declaration MrDocs points at. If that declaration is
   produced by a macro and a comment on the macro site cannot attach to the
   expanded symbols, expand or replace that call site with explicit documented
   declarations only when necessary — do not rewrite the library's macro system.
5. Missing third-party headers may need a small **shim** under the project's docs
   include path (`docs/shims/…`, or `missing-include-shims` /
   `missing-include-prefixes` in the config). Add the smallest stub that lets
   parsing continue; look at how this project or other demos already shim
   dependencies. Prefer shims over pulling an entire dependency tree into the
   docs build. If the stub refers to something that is not in the project, you can leave it undocumented (or add a brief comment if you want to avoid a warning). If the stub is for a symbol that is in the project, document it as you would any other symbol and mark it as @seebelow.

## When you are done

Summarize briefly which symbols you documented. State clearly that this was
**one capped batch**, and that more warnings likely remain under `--max-errors`.

## Diagnostics for this round

{report}
