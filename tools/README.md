# tools/

User-facing tools built on mrdocs (`libs/` is the opposite: libraries the
project depends on). Most link the mrdocs library and consume its public API,
but a tool can also be a script that drives the `mrdocs` executable. Either
way, tools are meant for the project's users, unlike `utils/`, which holds
developer and build tooling the project never ships.

## Layout
- `tools/<tool>/` is one tool. A compiled tool has its own recursive layout:
  its own `src/`, its own `CMakeLists.txt`, and optionally its own `tests/`.
  A script tool is just its files, with no build step.
- A compiled tool consumes the library through its public API (`<mrdocs/...>`).
  Reaching into library private headers is a smell to promote-or-refactor.

## Tools
- `mrdocs/` — the command-line documentation generator (the `mrdocs`
  executable). Its target is defined locally in `tools/mrdocs/CMakeLists.txt`
  and wired in from the root via `add_subdirectory`.
- `fix-docs/` — a Python tool that drives an AI agent and the `mrdocs`
  executable to bring a project's documentation up to date. See
  `tools/fix-docs/README.md`.
