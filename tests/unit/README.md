# tests/unit/

The unit-test suite (one executable, `mrdocs-unit-tests`). The directory tree
mirrors the module tree of the code under test: `ADT/`, `AST/`,
`Engines/`, `Extensions/`, `Generators/`, `Metadata/`, `Support/` (themed like the
Support module). A test for `<module>/Foo` lives at `tests/unit/<module>/Foo.cpp`.

The rules, the command and an example test are in the
[Testing & fixtures](https://mrdocs.com/docs/mrdocs/develop/contribute/testing.html#unit-tests)
page of the contributor guide.
