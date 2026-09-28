// Given: `leaf : mid : detail::grand_base`, where only the grandparent
//   declares a hidden friend, `operator==(grand_base, grand_base)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined; `mid` and `leaf` are documented.
// Expect: `lib::operator==(mid, mid)` under `mid` and
//   `lib::operator==(leaf, leaf)` under `leaf`: the page-less friend is copied
//   at every level it reaches.

namespace lib {

namespace detail {

/// Implementation-defined grandparent with a hidden friend.
struct grand_base
{
    /// Compares two `grand_base` values.
    friend bool operator==(grand_base, grand_base) { return true; }
};

}

/// Middle class deriving from the grandparent.
struct mid : detail::grand_base
{
};

/// Leaf class; the grandparent's friend reaches it through `mid`.
struct leaf : mid
{
};

}  // namespace lib
