# Getting started: compilation database

This example demonstrates MrDocs reading a `compile_commands.json` directly,
with no build system involvement. The file is handwritten and lives next
to the MrDocs configuration.

## Layout

```
compilation-database/
├── include/geo/point.hpp          public header
├── src/point.cpp                  source file
└── docs/
    ├── compile_commands.json      hand-written compilation database
    └── mrdocs.yml                 MrDocs configuration
```

## Running

```bash
mrdocs --config=docs/mrdocs.yml
```

That's it. No CMake. No build step. MrDocs reads
`docs/compile_commands.json`, expands `${MRDOCS_SOURCE_ROOT}` (see below),
and parses `src/point.cpp` with the recorded flags.

## Portable paths in a checked-in `compile_commands.json`

The standard `compile_commands.json` format requires absolute paths in the
`directory` field and in `file`/`command`/`arguments`. That works for a
file your build system regenerates locally, but it's a problem for a file
you want to check in: the next person's working copy lives at a different
absolute path.

MrDocs adds one extension: any occurrence of the literal token
`${MRDOCS_SOURCE_ROOT}` inside the JSON is replaced with the absolute path
of the project's `source-root` (the same value you set in `mrdocs.yml`)
when MrDocs loads the file. That lets you check in a single
`compile_commands.json` that works for every collaborator.

## When to use this pattern

* Tiny projects that don't have a build system at all.
* Documentation builds in CI where you'd rather check a database in than
  install build tools.
* Teaching examples where adding CMake distracts from the point.

For most real projects you'd let your build system produce the database
(`cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, Bazel's
`bazel-compile-commands-extractor`, Meson by default, `bear` for plain
Make, etc.) and point `compilation-database` at the produced file. The
`cmake` and `cmake-header-only` patterns walk that workflow when the build
system is CMake.
