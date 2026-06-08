# Getting started: scanned compilation database

This example demonstrates the scanned pattern: no build system, no
`compile_commands.json`. MrDocs walks the `input:` directories, synthesizes
a compilation database from the headers, and parses each as its own
translation unit with a default set of compile flags.

## Layout

```
scanned/
├── include/geo/point.hpp    public header
└── docs/mrdocs.yml          MrDocs configuration
```

## Running

```bash
mrdocs --config=docs/mrdocs.yml
```

That's all. No build system. No pre-build step.

## Same as cmake-header-only?

Functionally yes: both end up scanning headers and synthesizing a database.
The difference is where the configuration lives. If your project already uses
CMake for everything else, the xref:../cmake-header-only/README.md[cmake-header-only]
pattern keeps the documentation build inside CMake's lifecycle. If the project
has no CMake at all, the scanned pattern keeps things minimal.
