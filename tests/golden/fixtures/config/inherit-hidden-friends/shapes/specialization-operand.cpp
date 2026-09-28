// Given: `derived : detail::base<int>`, where the friend
//   `operator==(base<T>, base<T>)` spells out the specialization.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined, so `base<int>` has no page to link to.
// Expect: `lib::operator==(derived, derived)` listed under `derived`: the
//   `base<int>` operand is rewritten because its primary has no page.

namespace lib {

namespace detail {

/// Implementation-defined base template whose friend takes the specialization.
template <class T>
struct base
{
    /// Compares two base values.
    friend bool operator==(base<T>, base<T>) { return true; }
};

}

/// Derives from a specialization of the implementation-defined base.
struct derived : detail::base<int>
{
};

}  // namespace lib
