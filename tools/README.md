# tools/

Executables built on top of the mrdocs library. Tools *depend on* the library
(`libs/` is the opposite: libraries the project depends on). Keeping tools here
is what lets `src/` hold the library and nothing else.

## Layout
- `tools/<tool>/` is one tool with its own recursive layout: its own
  `src/`, its own `CMakeLists.txt`, and optionally its own `tests/`.
- A tool consumes the library through its public API (`<mrdocs/...>`). Reaching
  into library private headers is a smell to promote-or-refactor.

## Tools
- `mrdocs/` — the command-line documentation generator (the `mrdocs`
  executable). Its target is defined locally in `tools/mrdocs/CMakeLists.txt`
  and wired in from the root via `add_subdirectory`.
