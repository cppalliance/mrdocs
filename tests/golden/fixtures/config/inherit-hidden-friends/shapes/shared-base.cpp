// Given: `derived_a : detail::base` and `derived_b : detail::base`, one
//   page-less base with `operator==(base, base)` shared by two classes.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: two distinct copies, `lib::operator==(derived_a, derived_a)` under
//   `derived_a` and `lib::operator==(derived_b, derived_b)` under `derived_b`,
//   each with its own id.

namespace lib {

namespace detail {

/// Implementation-defined base shared by several derived classes.
struct base
{
    /// Compares two base values.
    friend bool operator==(base, base) { return true; }
};

}

/// First derived class.
struct derived_a : detail::base
{
};

/// Second derived class.
struct derived_b : detail::base
{
};

}  // namespace lib
