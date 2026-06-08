# Getting started: regular CMake library

This example demonstrates a regular CMake compiled library (headers plus source files)
documented by MrDocs. MrDocs configures the project itself in a temporary
build directory and reads the `compile_commands.json` CMake produces.

## Layout

```
cmake/
├── CMakeLists.txt           with a MRDOCS_BUILD guard for docs-only targets
├── include/geo/point.hpp    public header
├── src/point.cpp            source file
└── docs/mrdocs.yml          MrDocs configuration
```

## Running

```bash
mrdocs --config=docs/mrdocs.yml
```

You don't need to build the project first. MrDocs invokes CMake, reads the
`compile_commands.json` it produces in the temporary build directory, and
parses every translation unit.

## The MRDOCS_BUILD flag

MrDocs sets the CMake variable `MRDOCS_BUILD` before invoking CMake. Use it
to skip targets that aren't part of the public API (test runners, internal
tools) or to enable documentation-only targets. The check in this example's
`CMakeLists.txt` is a no-op placeholder; remove it if you don't need it.
