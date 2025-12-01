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
