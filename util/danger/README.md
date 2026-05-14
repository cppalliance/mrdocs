# Danger.js checks for MrDocs

This directory contains the Danger.js rules and fixtures used in CI to add scoped summaries and hygiene warnings on pull requests.

## What runs in CI

- Danger executes via `npx danger ci --dangerfile util/danger/dangerfile.ts` in the `Repo checks` job.
- That job runs on `pull_request_target` so forked PRs get comments/status updates using the base-repo token without executing forked code.
- Permissions are scoped to comment and set statuses (`contents: read`, `pull-requests: write`, `issues: write`, `statuses: write`).
- The job lives in `.github/workflows/ci.yml` after the matrix generator so matrix jobs stay first in the UI.

## Local usage

```bash
npm --prefix util/danger ci           # install dev deps (without touching the repo root)
npm --prefix util/danger test         # run Vitest unit tests for rule logic
npm --prefix util/danger run danger:local  # print the fixture report from util/danger/fixtures/sample-pr.json
npm --prefix util/danger run danger:scope-map  # emit JSON mapping every tracked file to its Danger scope
npm --prefix util/danger run danger:ci     # run Danger in CI mode (requires GitHub PR context)
```

## Key files

- `logic.ts` — Pure rule logic: scope mapping, conventional commit validation, size and hygiene warnings.
- `runner.ts` — Minimal Danger runtime glue (fetches commits/files and feeds `logic.ts`).
- `dangerfile.ts` — Entry point passed to `danger ci`; keep thin.
- `fixtures/` — Sample PR payloads for local runs; update alongside rule changes.
- `logic.test.ts` — Vitest coverage for the rule logic.
- `package.json`, `package-lock.json`, `tsconfig.json` — Localized Node setup to avoid polluting the repository root.

## Conventions

- Scopes reflect the MrDocs tree: `source`, `tests`, `golden-tests`, `docs`, `ci`, `build`, `tooling`, `third-party`, `other`.
- Conventional commit types allowed: `feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert`.
- Non-test commit size notice triggers at 2000 lines of churn (tests and golden fixtures ignored) and is informational.
- Aggregate PR source churn triggers a warning above 5000 lines (much more generous than the per-commit limit, since well-sliced commits can still amount to a large overall change).
- PR description length is checked against a log-scaled floor (`80 * log2(churn)` characters). Tiny diffs need ~80 chars; ~1k-line changes need ~800; ~30k-line changes need ~1200. HTML comments in the body (e.g. PR template scaffolding) are stripped before measurement.
- Feature PRs (`feat:` commits, `feat` PR title, or `feature` label) without any `docs/` change get a light warning. Opt out with the `no-docs-needed` label or a `[skip danger docs]` marker in the PR body.
- The PR template (`.github/pull_request_template.md`) mirrors these rules; following it tends to satisfy all of them automatically. Section headers from the template (any level) that are missing from the PR body are reported as an informational note rather than a warning.

## Updating rules

- Edit `logic.ts`, refresh `fixtures/` as needed, and run `npm test`.
- Keep warnings human-readable; prefer `warn()` over `fail()` until the team decides otherwise.
