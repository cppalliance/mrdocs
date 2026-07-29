# libs/

Self-contained libraries the main mrdocs library (or the tests) depend on, each
a cohesive component consumed through a stable public surface. This mirrors the
Pitchfork/mustache convention: a library per concern, not a single grab-bag.

Each component owns its `include/`, `src/`, and `tests/`, has its own
`CMakeLists.txt`, and is wired in from `libs/CMakeLists.txt`. Header-only
components are `INTERFACE` libraries; compiled components are `STATIC`
libraries. A component never depends on `mrdocs-core`; the dependency arrow
always points from `mrdocs-core` into `libs/`.

## The dependency rule

The base libraries here (`dom`, `handlebars`) must depend ONLY on the C++
standard library, the `polyfill` library, and each other where it makes sense
(`handlebars` operates on `dom` values). They must NOT depend on
`mrdocs/Support/*` or anything in `mrdocs-core`, which keeps them genuinely
reusable and independently testable. Each library's tests live in its own
`tests/` and link only that library (plus `test_suite`), so a failure points at
one component.

## polyfill vs. Support

`polyfill/` holds stand-ins for standard-library features that are not yet
available on every compiler we support; a polyfill is temporary and gets deleted
once its feature is available everywhere. `mrdocs/Support/` holds permanent
project utilities with no standard-library equivalent. See `polyfill/README.md`
for the full doctrine and naming rules.

## Components
- `test_suite/` — the unit-test framework (assertions, `unit_test_main`,
  `diff`), consumed as `<test_suite/...>` by the test executables.
- `polyfill/` — the standard-library stand-ins described above; an INTERFACE
  library (`mrdocs::polyfill`), header-only, depending only on std.
- `dom/` — the document object model (`mrdocs::dom`), a compiled STATIC library
  depending only on std + polyfill. Tested independently by `mrdocs-dom-tests`.
- `handlebars/` — the Handlebars template engine (`mrdocs::handlebars`), a
  compiled STATIC library depending only on std + polyfill + dom.
