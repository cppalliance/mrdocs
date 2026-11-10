# Getting started: compilation database

MrDocs reading a handwritten `compile_commands.json` directly, with no CMake and no build step. The database and the config are in `docs/`, the code is `include/geo/point.hpp` and `src/point.cpp`, and MrDocs parses the source with the flags the database records.

```
mrdocs --config=docs/mrdocs.yml
```

A checked-in database cannot hold absolute paths. MrDocs replaces every `${MRDOCS_SOURCE_ROOT}` in the file with the `source-root` of the project when it loads it, so one file works in every working copy. The Usage page lists the tools that produce a database for you, and `bear` records one for plain Make.

Use this for tiny projects, CI documentation builds that should not install build tools, and teaching examples. For most projects let the build system produce the database, as the `cmake` and `cmake-header-only` examples do.
