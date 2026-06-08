# Getting started: header-only CMake library

This example demonstrates a header-only CMake library documented by MrDocs.
The CMakeLists.txt sets an `MRDOCS_INPUT_HEADER_ONLY` option that MrDocs
reads to switch into header-scan mode (no translation units to compile;
each header is its own TU).

## Layout

```
cmake-header-only/
├── CMakeLists.txt           sets MRDOCS_INPUT_HEADER_ONLY
├── include/geo/point.hpp    public header
└── docs/mrdocs.yml          MrDocs configuration
```

## Running

```bash
mrdocs --config=docs/mrdocs.yml
```

MrDocs invokes CMake, sees `MRDOCS_INPUT_HEADER_ONLY=ON`, and switches to
scanning `input:` directories rather than compiling source files.

## Why not the scanned pattern?

The xref:scanned[scanned] pattern does the same scanning without involving
CMake at all. Use header-only-via-CMake when your project already uses CMake
(for consumers, for testing, for installation rules) and you want MrDocs to
fit in. Use the scanned pattern when the project has no build system.
