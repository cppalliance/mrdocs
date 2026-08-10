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
includes. `python report.py --check` re-runs the capture and fails if
`report.txt` has drifted; in the MrDocs build the `documentation-check-report`
test guards it, and `cmake --build build --target mrdocs-documentation-check-report`
rewrites it.

See `generators/noop.adoc` in the documentation for the full write-up.
