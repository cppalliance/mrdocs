# include/mrdocs/Support/

Small, library-like utilities, organized into themed subdirectories (each like
a tiny Boost library) rather than one flat grab-bag. The grab-bag is what
incentivized over-long single-purpose headers; a theme can spread a concern
across as many files as it needs.

## Themes
- `Container/` — `Algorithm`, `RangeFor`.
- `TypeTraits/` — `TypeTraits`, `Concepts`, `Visitor`, `any_callable`.
- `String/` — `String`, `StringList`, `SplitLines`, `Parse`.
- `Error/` — `Error`, `Expected`, `Assert`.
- `Reflection/` — Boost.Describe-based helpers: `Describe`, `DescribeKinds`,
  `EnumToString`, `CompareReflectedType`, `MapReflectedType`,
  `MergeReflectedType`.
- `Concurrency/` — `ThreadPool`, `ExecutorGroup`, `unlock_guard`.
- `Filesystem/` — `Path`, `Glob`.
- (root) `Report`, `ScopeExit`, `source_location` — cross-cutting bits that
  don't belong to one theme.

## Not here
Engines (JavaScript, Lua, Handlebars) are NOT Support utilities; they live in
`mrdocs/Engines/`. Support holds only tiny-library-like helpers.
