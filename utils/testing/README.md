# utils/testing/

Scripts that run the test suites.

- `run_all_tests.py` configures the preset that best matches the OS and builds it with `-Werror -Wall` (`/WX /W4` on MSVC), as CI does. A release preset with `LLVM_ROOT` set and no named compiler wins.
- The same script then runs the golden tests first and the rest of ctest, installs into `install/<preset>`, and builds the docs from that install through `MRDOCS_ROOT`.
- `run_ci_with_act.py` runs the jobs of `.github/workflows/ci.yml` locally through act with a `pull_request` event. It installs act and keeps its caches under `local/act/`, which git ignores. Secrets default to placeholders, so publishing steps stay skipped.

```
python utils/testing/run_all_tests.py [--preset NAME] [--no-strict] [--update-golden] [--skip-docs]
python utils/testing/run_ci_with_act.py [--list-jobs] [--jobs JOB ...] [--image RUNNER=IMAGE] [--dry-run] [--reuse]
```

`--update-golden` refreshes the golden fixtures first, only for intended output changes. `--reuse` keeps the containers alive between job batches. The [testing page](../../docs/modules/ROOT/pages/contribute/testing.adoc) explains the suites.
