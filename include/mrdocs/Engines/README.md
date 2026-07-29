# include/mrdocs/Engines/

Scripting and templating engines used by mrdocs. These are NOT Support
utilities (Support holds tiny-library-like helpers); they are larger,
self-contained engines, so they get their own module.

## Contents
- `JavaScript.hpp` — the JavaScript engine wrapper.
- `Lua.hpp` — the Lua engine wrapper.
- `Handlebars.hpp` — the Handlebars template engine.

Private implementation (including `LuaHandlebars`, the `Console`/`StdGlobals`
runtime helpers) lives under `src/mrdocs/Engines/`.
