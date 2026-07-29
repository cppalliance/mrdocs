# src/mrdocs/Engines/

Implementations of the scripting and templating engines whose public wrappers
live in `include/mrdocs/Engines/`: the JavaScript engine (JerryScript-backed),
the Lua engine, and the Handlebars template engine, plus their private runtime
helpers (`Console`, `StdGlobals`, `LuaHandlebars`). These are not Support
utilities; they are large, self-contained engines.
