// Given: `both : detail::left, detail::right`, where each base declares a
//   same-shaped friend, `operator==(left, left)` and `operator==(right, right)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: a single `lib::operator==(both, both)` under `both`. Both copies
//   substitute to the same signature, and the second is dropped as a
//   duplicate of the first.

namespace lib {

namespace detail {

/// First implementation-defined base with a hidden friend.
struct left
{
    /// Compares two `left` values.
    friend bool operator==(left, left) { return true; }
};

/// Second implementation-defined base with a friend of the same form.
struct right
{
    /// Compares two `right` values.
    friend bool operator==(right, right) { return true; }
};

}

/// Derives from both bases.
struct both : detail::left, detail::right
{
};

}  // namespace lib
