// Given: `derived : detail::base`, where `detail::base` declares the hidden
//   friend `operator==(base, base)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined, so `base` and its friend have no page.
// Expect: `lib::operator==(derived, derived)`, a copy that notes it was
//   declared in a base, listed under `derived`'s Non-Member Functions.

namespace lib {

namespace detail {

/// Implementation-defined base with a hidden friend.
struct base
{
    /// Compares two base values.
    friend bool operator==(base, base) { return true; }
};

}

/// Derives from the implementation-defined base.
struct derived : detail::base
{
};

}  // namespace lib
