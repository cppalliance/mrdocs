# Documentation check (no-op generator)

This example runs MrDocs as a documentation linter. The `noop` generator
extracts the whole corpus and writes nothing, so the run produces only
diagnostics. With `warn-as-error: true` in `mrdocs.yml`, any documentation
problem (an undocumented parameter, a broken reference, a malformed comment)
exits non-zero, which is exactly what a CI job needs.

## Run the check

```bash
mrdocs --config=mrdocs.yml
```

`include/geo/area.hpp` leaves `rectangle_area`'s parameters undocumented on
purpose, so the check catches it and exits non-zero with a per-symbol report.
Document the parameters and it passes silently.

## Wire it into CTest

`CMakeLists.txt` finds MrDocs with `find_package(mrdocs)` and registers the
check as a CTest test, so a forgotten `@param` fails `ctest` like any unit test.
It builds either standalone or as part of MrDocs itself: standalone it finds an
installed MrDocs; in the MrDocs build the just-built `mrdocs::mrdocs` is reused.

```bash
cmake -S . -B build && ctest --test-dir build
```

## Keep the page's report real

The No-op page shows the report this header produces. `report.py` runs the same
check and writes the tool's diagnostic messages to `report.txt`, which the page
includes: next to the script, or in the directory given by `--output=<dir>`. In
the MrDocs build, the
`mrdocs-example-generators-noop-documentation-check-report` test compares a
fresh capture with the committed `report.txt`, and building the
`mrdocs-update-example-generators-noop-documentation-check-report` target, or
`mrdocs-update-examples` for every example, rewrites it.

See `generators/noop.adoc` in the documentation for the full write-up.
