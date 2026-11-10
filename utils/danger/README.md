# Danger.js checks for MrDocs

The Danger.js rules CI runs on pull requests: scope summaries, conventional commit validation, and size and hygiene warnings. The `repo-checks` job of `.github/workflows/pr-target-checks.yml` runs them on `pull_request_target`, so forked PRs get comments without running forked code. The Danger.js checks section of the `utils/` layout page lists the rules and key files.

The rule logic is in `logic.ts`. A rule change also refreshes `fixtures/` and passes the tests.

```
npm --prefix utils/danger ci
npm --prefix utils/danger test
npm --prefix utils/danger run danger:local
npm --prefix utils/danger run danger:scope-map
```
