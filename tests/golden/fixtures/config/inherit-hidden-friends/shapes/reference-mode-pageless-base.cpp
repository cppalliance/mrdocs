// Given: `derived : detail::base`, where the base declares the hidden friend
//   `operator==(base, base)`.
// Setting: `inherit-hidden-friends: reference`; `lib::detail` is
//   implementation-defined, so the friend has no page to link to.
// Expect: nothing under `derived`. `reference` never copies, and a reference
//   to a page-less friend would dangle, so it is skipped.

namespace lib {

namespace detail {

/// Implementation-defined base with a hidden friend.
struct base
{
    /// Compares two base values.
    friend bool operator==(base, base) { return true; }
};

}

/// Derives from the implementation-defined base in reference mode.
struct derived : detail::base
{
};

}  // namespace lib
