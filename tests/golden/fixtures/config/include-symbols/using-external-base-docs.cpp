// Regression test for #1119: inheriting from an excluded/external base class
// must not emit missing-documentation diagnostics for the base members.
// The YAML config for this fixture only includes an unrelated namespace, so
// `ns::Foo` is external; Baz inherits it and should not trigger diagnostics.
namespace ns {
struct Foo {
    /// bar
    int
    bar();
};
} // namespace ns

/// project namespace
namespace mrdocs {
/// Baz
struct Baz : ns::Foo {};
} // namespace mrdocs
