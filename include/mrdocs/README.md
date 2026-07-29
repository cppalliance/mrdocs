# include/mrdocs/

The public API surface of MrDocs. Everything here is installed and reachable by
consumers via `<mrdocs/...>`; nothing under `src/` is. Private implementation
headers live beside their sources in `src/mrdocs/` and are never included from
here.

## Modules
- `ADT/` value types and small containers (`Optional`, `Polymorphic`, ...).
- `Config/` configuration types and the generated settings surface.
- `Dom/` the document object model used by templates and generators.
- `Metadata/` the extracted-symbol model (the core domain types).
- `Support/` small, library-like utilities, organized into themed subdirs.
- `Engines/` the scripting/templating engines (JavaScript, Lua, Handlebars).

Top-level headers (`Corpus.hpp`, `Config.hpp`, `Generator.hpp`, `Metadata.hpp`,
`Dom.hpp`, ...) are the umbrella entry points for those modules.
