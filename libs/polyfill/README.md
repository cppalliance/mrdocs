# libs/polyfill/

Stand-ins for standard-library features that are not yet available on every
compiler this project supports. See `../README.md` for the full doctrine; the
short version:

- A polyfill is **temporary**. When its standard feature is available across all
  our compilers, we **delete** the polyfill and move call sites to the standard
  one. That temporariness is what separates `polyfill/` from `mrdocs/Support/`
  (which holds permanent utilities with no standard equivalent).
- Polyfills mirror the standard library exactly, **`snake_case` and all**
  (`mrdocs::polyfill::expected`, `unexpected`, `source_location`), so deleting a
  polyfill is a pure find-and-replace to the `std::` spelling. The mrdocs-facing
  `mrdocs::Expected` / `dom::Expected` / `handlebars::Expected` aliases keep the
  project's PascalCase; they are thin aliases over the snake_case polyfill.
- This library depends ONLY on the standard library. It is the only dependency
  (besides std) that `dom` and `handlebars` are allowed.

Header-only, so it is a CMake `INTERFACE` library (`mrdocs::polyfill`). Tests
live in `tests/` and link only this library plus `test_suite`.

## Current polyfills
- `<mrdocs/polyfill/source_location.hpp>` — `std::source_location` backport;
  becomes an alias to `std::source_location` when `<source_location>` is present.
- `<mrdocs/polyfill/expected.hpp>` — `std::expected` polyfill as
  `mrdocs::polyfill::expected<T, E>` (+ `unexpected`, `bad_expected_access`,
  `unexpect`). No default error type and no `Error` dependency. The
  mrdocs-specific `mrdocs::Expected<T, E = Error>` is just an alias defined in
  `<mrdocs/Support/Error/Expected.hpp>`, which also keeps the Error-coupled
  `detail::failed`/`error` helpers and the `MRDOCS_TRY`/`MRDOCS_CHECK` macros.
  The implementation details (trait predicates, the reinit/guard helpers) live in
  `<mrdocs/polyfill/detail/expected.hpp>` so `expected.hpp` reads as the
  interface. Fully STD-ONLY: the C++23 `reference_constructs_from_temporary_v`
  trait it needs is a real polyfill in `<mrdocs/polyfill/type_traits.hpp>`
  (`mrdocs::polyfill`, mirroring `<type_traits>`). The polyfill includes nothing
  outside `<mrdocs/polyfill/...>` + the standard library.
- `<mrdocs/polyfill/type_traits.hpp>` — the C++23 reference-from-temporary
  traits (`reference_constructs_from_temporary_v`,
  `reference_converts_from_temporary_v`); aliases to the `std` versions when
  `<type_traits>` provides them.

## Planned (incremental extraction)

Design (per maintainer): `polyfills` holds ONLY genuine std stand-ins. Types
that merely look standard but deviate (compact `Optional`, `Polymorphic`) are
NOT polyfills and stay in `mrdocs/Support` / `mrdocs/ADT`; dom and handlebars do
not need them. Small remaining Support deps (String/Describe/RangeFor/Path) are
small enough to replicate per consuming library rather than share.

- `expected` (std::expected): the polyfill goes here as
  `mrdocs::polyfill::expected<T, E>` (no default error type, no `Error`
  dependency). The mrdocs-specific `mrdocs::Expected<T, E = Error>` is then just
  an alias defined in `mrdocs/Support/Error`, supplying the `Error` default. The
  `MRDOCS_TRY`-style macros are mrdocs extensions and stay in the Error dir.
  ANALYSIS: the `Expected` CLASS template itself is std-only except the
  `= Error` default; the only `Error` coupling is in the `detail::failed`/
  `detail::error` helpers (Expected.hpp ~330-384) that back the `MRDOCS_TRY`
  macros. Those helpers + macros are extensions and stay in the Support/Error
  wrapper alongside the `using Expected = polyfill::expected<T, Error>` alias.
  PRECISE RECIPE (mapped against the current file):
    - The class body + both .ipp specs are std-only EXCEPT `MRDOCS_ASSERT`;
      replace those with `<cassert>` `assert(...)` in the polyfill copy.
    - The ONLY Error-coupled region is the `detail` block at ~lines 333-384
      (`failed`/`error`) plus the `MRDOCS_TRY`-family macros immediately after
      it (~through ~480). That contiguous region STAYS in the Support/Error
      wrapper. The other two `detail` blocks (~149 and ~480) are std-only and
      move with the class.
    - Polyfill file (mrdocs::polyfill, std-only): fwd decl WITHOUT `= Error`,
      detail@149, detail@480, the class (drop `= Error`), Unexpected,
      BadExpectedAccess, unexpect_t, deduction guides, isExpected, and the two
      .ipp specs. Includes only std (+ <mrdocs/polyfill/source_location.hpp>).
    - Wrapper (include/mrdocs/Support/Error/Expected.hpp): includes the polyfill
      + Error + Assert; defines `template<class T,class E=Error> using Expected
      = polyfill::Expected<T,E>;` and the `Unexpected`/`unexpect` aliases; keeps
      detail@333 (failed/error) and the MRDOCS_TRY macros.
    - ~15 files include <mrdocs/Support/Error/Expected.hpp> and keep working via
      the alias (no churn). Verify the self-doc build too (it compiles all public
      headers; the polyfills include dir is already on its path).
- `source_location` (done above).

NOT polyfills (stay in Support/ADT): `Optional` (compact optional, diverges from
std), `Polymorphic` (diverges from std), `Error` (permanent domain type),
`Platform` (build config).
