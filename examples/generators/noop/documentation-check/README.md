# Documentation check (no-op generator)

MrDocs as a documentation linter. The `noop` generator extracts the corpus and writes nothing, so the run only reports diagnostics. With `warn-as-error: true` in `mrdocs.yml`, a documentation problem makes it exit non-zero, which is what a CI job needs. Undocumented parameters, broken references and malformed comments all count.

`include/geo/area.hpp` leaves the parameters of `rectangle_area` undocumented on purpose, so the check fails until they are documented.

```
mrdocs --config=mrdocs.yml
cmake -S . -B build && ctest --test-dir build
```

`CMakeLists.txt` registers the check as a CTest test, standalone against an installed MrDocs or inside the MrDocs build. `report.py` writes the diagnostics to `report.txt`, which the No-op page includes. `python report.py --check` fails when the file is out of date.

See `generators/noop.adoc` in the documentation.
