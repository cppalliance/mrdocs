# tests/

Test suites for the mrdocs library, each its own executable target (not one
binary behind flags). Defined locally in `tests/CMakeLists.txt`, wired in from
the root via `add_subdirectory`. Both suites link the `test_suite` framework
from `libs/test_suite`.

## Suites
- `unit/` — unit tests, mirroring the library's module layout under
  `src/mrdocs/` (`unit/AST/`, `unit/Gen/`, `unit/Metadata/`, `unit/Support/`,
  `unit/Engines/`, `unit/Extensions/`, `unit/ADT/`). Built into the
  `mrdocs-unit-tests` executable (`UnitMain.cpp` is the entry point). Uses the
  public API plus `test_suite`, and reaches library internals it exercises as
  `<mrdocs/...>` (see the private include dirs in `unit/CMakeLists.txt`). Run
  via `ctest -R mrdocs-unit-tests`.
- `golden/` — the reference-output harness (`TestRunner`, `Comparison`,
  `TestArgs`, `TestMain`). Built into the `mrdocs-golden-tests` executable. It
  walks `tests/golden/fixtures`, rendering each test with the generator(s)
  declared in its own config and diffing against the committed fixtures. The
  `mrdocs-{test,create,update}-test-fixtures-all` targets drive create/update.
