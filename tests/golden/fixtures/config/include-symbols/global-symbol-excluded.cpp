// Regression test for #1121: limiting include-symbols to a namespace should
// not extract global symbols that do not match the pattern.
struct Baz {};

/// Included namespace
namespace mrdocs {
/// Included record
struct Foo {};
} // namespace mrdocs
