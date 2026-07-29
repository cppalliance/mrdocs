# tests/golden/

The golden (reference-output) test suite (`mrdocs-golden-tests`). The runner
walks the shared corpus under `tests/golden/fixtures/`, renders each test with
the generator(s) declared in its own configuration, and compares against the
checked-in reference output. Fixtures are driven only through the custom
targets (one per action), never edited by hand:
`mrdocs-test-test-fixtures-all` (validate against the fixtures),
`mrdocs-create-test-fixtures-all` (write missing fixtures), and
`mrdocs-update-test-fixtures-all` (overwrite fixtures with current output).
