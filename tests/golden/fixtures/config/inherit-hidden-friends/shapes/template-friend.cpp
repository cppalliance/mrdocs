// Given: `derived : detail::base`, where the base declares the hidden
//   function-template friend `template <class T> operator==(base, T const&)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: `template <class T> lib::operator==(derived, T const&)` listed under
//   `derived`: the copy keeps the template head and rewrites only the operand
//   that named the base.

namespace lib {

namespace detail {

/// Implementation-defined base with a hidden function-template friend.
struct base
{
    /// Compares a base value with anything.
    template <class T>
    friend bool operator==(base, T const&) { return true; }
};

}

/// Derives from the implementation-defined base.
struct derived : detail::base
{
};

}  // namespace lib
