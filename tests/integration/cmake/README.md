# tests/integration/cmake

A consumer project that finds an installed MrDocs with `find_package(mrdocs)`. The top `CMakeLists.txt` finds the package once, and one subdirectory covers each way of using it:

* `executable/` runs the `mrdocs` tool on a small example project (`include/`, `src/`, `mrdocs.yml`, `docs/`). The `consumer-docs` target generates its reference.
* `plugin/` builds a plugin with `mrdocs_add_plugin` and loads it into the tool (`consumer-plugin-loads`). On macOS it also checks that the helper refuses a deployment target above the one the package was built with.
* `library/` links `mrdocs::mrdocs-core` into a program (`consumer-runs`) and checks that `mrdocs_ensure_runtime_dlls` skips DLLs that are already in reach (`consumer-runtime-dlls-skip-identical`).

The test proves the installed package works from outside the source tree. CI runs it against the install prefix of each build, including the shared-library builds, and the Releases workflow runs it against the release packages.

```
cmake -S tests/integration/cmake -B build/cmake-consumer -D mrdocs_ROOT=<install prefix>
cmake --build build/cmake-consumer
ctest --test-dir build/cmake-consumer --output-on-failure
```
