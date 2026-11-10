# tools/cmake

The CMake helper for programs that link Mr.Docs, and the script it runs.

- `mrdocs-helpers.cmake` is the entry point: it includes one file per helper.
- `mrdocs_ensure_runtime_dlls.cmake` defines `mrdocs_ensure_runtime_dlls(<target>)`, which a program that links a shared Mr.Docs calls so Windows finds the DLLs next to it.
- `mrdocs_add_plugin.cmake` defines `mrdocs_add_plugin(<target> <sources>...)`, which builds a plugin.
- `LinkOrCopyDlls.cmake` is the script that `mrdocs_ensure_runtime_dlls` runs after the build, in `-P` mode.

All of them install next to `mrdocs-config.cmake`, which includes the entry point, so `find_package(mrdocs)` gives a consumer the function. The Mr.Docs build includes the same helper from here.

The build's own CMake helpers (`helpers.cmake`, `install.cmake`, `release-linux.cmake`) are in `utils/cmake/`, and the install never includes them.
