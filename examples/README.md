# examples/

Worked examples of using MrDocs, each a self-contained project you can run.

## Contents
- `getting-started/` — the basic setups (regular CMake, header-only, compilation-database, scanned).
- `configuration/` — configuration features (diagnostics, inputs, parsing, and an overview).
- `dependencies/` — handling dependencies (find, accept-missing, shim files and snippets).
- `generators/` — data-driven and script-driven output generators.
- `library/` — building against the MrDocs library API directly.
- `third-party/` — documenting real external libraries (fmt, mp-units, nlohmann-json, ...).

## Tests

The build registers each example it exercises with `mrdocs_add_example`,
which cmake/MrDocsExample.cmake defines and documents. Each gets a test,
`mrdocs-example-<name>`, that runs the example from its own directory and
fails if an output the example commits no longer matches the run; an
example that commits no output only checks that the run succeeds. When a
change to an output is intended, build `mrdocs-update-example-<name>`, or
`mrdocs-update-examples` for every example, to rewrite the committed files.

The exception is `library/breaking-changes`, a program built against the
MrDocs library rather than a run of the tool, which keeps a test of its own.
