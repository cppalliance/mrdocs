# utils/

Contributor and developer tooling: scripts for building, testing, linting, code
generation, and CI checks. These are maintenance helpers, not something the
project ships to its users (user-facing executables live in `tools/`).

## Contents
- `bootstrap/` — the `bootstrap.py` dependency/build setup tool and its Python tests.
- `codegen/` — code generators (config-info, YAML schema, and `trang.jar` for the XML schema).
- `danger/` — the Danger.js pull-request size and hygiene checks run in CI.
- `docs/` — docs-tooling checks and generators, such as the bootstrap-options generator.
- `linting/` — formatting helpers (`reformat.py` / clang-format) and checkers
- `testing/` — the `run_all_tests.py` test runner and `run_ci_with_act.py`.
