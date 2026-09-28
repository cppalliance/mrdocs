// Given: `derived : detail::base`, where the friend `operator==(base, base)`
//   also has a matching declaration at namespace scope.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: nothing under `derived`. A friend with a namespace-scope declaration
//   is found by ordinary lookup, so it is not a hidden friend and the option
//   does not apply to it.

namespace lib {

namespace detail {

/// Implementation-defined base whose friend is also declared at namespace scope.
struct base
{
    /// Compares two base values.
    friend bool operator==(base, base);
};

/// The matching namespace-scope declaration: the friend is not hidden.
bool operator==(base, base) { return true; }

}

/// Derives from the base; nothing is inherited.
struct derived : detail::base
{
};

}  // namespace lib
