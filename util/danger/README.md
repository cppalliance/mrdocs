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
- Non-test commit size warning triggers around 800 lines of churn (tests and golden fixtures ignored).

## Updating rules

- Edit `logic.ts`, refresh `fixtures/` as needed, and run `npm test`.
- Keep warnings human-readable; prefer `warn()` over `fail()` until the team decides otherwise.

> Note: The Danger.js rules for MrDocs are still experimental, so some warnings may be rough or occasionally fire as false positives—feedback is welcome.
