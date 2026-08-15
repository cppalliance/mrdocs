# data/

Assets that ship alongside the binaries or feed the build system: everything the
project packages but does not compile. These subdirectories install into
`CMAKE_INSTALL_DATAROOTDIR` (equivalent to `/usr/local/share` on unix) and are
mirrored into the `share/` subtree; this README is not installed.

## Contents
- `mrdocs/` — runtime assets installed with MrDocs: `addons/` (Handlebars
  templates and generator helpers) and `headers/` (bundled libc stubs).
- `gdb/` — GDB pretty-printers for MrDocs types.
- `lldb/` — LLDB data formatters for MrDocs types.
