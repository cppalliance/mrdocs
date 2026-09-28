// Given: `derived : detail::base`, where the base declares the friend
//   `operator==(base, base)` without defining it.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: `lib::operator==(derived, derived)` listed under `derived`. A friend
//   is hidden when its only declarations are friend declarations; whether it
//   is defined in the class body does not matter.

namespace lib {

namespace detail {

/// Implementation-defined base whose friend is declared but not defined.
struct base
{
    /// Compares two base values.
    friend bool operator==(base, base);
};

}

/// Derives from the implementation-defined base.
struct derived : detail::base
{
};

}  // namespace lib
