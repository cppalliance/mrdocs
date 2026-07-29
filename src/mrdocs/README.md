# src/mrdocs/

The private implementation of the MrDocs library. The directory tree mirrors the
public module layout in `include/mrdocs/`: a public `include/mrdocs/X/Foo.hpp`
pairs with `src/mrdocs/X/Foo.cpp`, and private headers live beside their sources.

There is NO `-I src`. Private headers are included via quoted, relative paths
(`"Foo.hpp"`, `"../Support/Filesystem/Temp.hpp"`); public headers via
`<mrdocs/...>`. A private companion to a public header `X.hpp` is named
`XImpl.hpp`. Cross-module access goes through the public API, not private headers.

## Modules
- `AST/` Clang AST traversal and symbol extraction.
- `Metadata/` builds and finalizes the extracted-symbol model.
- `Dom/` the DOM implementation over the metadata.
- `Gen/` the output generators (one subdir per format).
- `Engines/` the scripting/templating engine implementations.
- `Extensions/` the extension mechanism and bindings.
- `Support/` private utility implementations, mirroring the public Support themes.
