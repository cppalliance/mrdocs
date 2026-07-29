# tests/unit/

The unit-test suite (one executable, `mrdocs-unit-tests`). The directory tree
mirrors the module tree of the code under test: `ADT/`, `AST/`, `Dom/`,
`Engines/`, `Extensions/`, `Gen/`, `Metadata/`, `Support/` (themed like the
Support module). A test for `<module>/Foo` lives at `tests/unit/<module>/Foo.cpp`.

`fixtures/` holds input data owned solely by unit tests (currently the Handlebars
templates), reached via the `MRDOCS_TEST_FILES_DIR` compile definition.
