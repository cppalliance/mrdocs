// Given: `leaf : detail::mid`, where `detail::mid : equality_comparable<mid>`
//   and the CRTP friend `operator==(Derived const&, Derived const&)`
//   instantiates to `operator==(mid const&, mid const&)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined, so `mid` and the CRTP base have no page.
// Expect: `lib::operator==(leaf const&, leaf const&)` listed under `leaf`: the
//   friend reaches `leaf` through the page-less intermediate, and the operand
//   naming that intermediate is rewritten to `leaf`.

namespace lib {

namespace detail {

/// Implementation-defined CRTP base whose friend takes the derived type.
template <class Derived>
struct equality_comparable
{
    /// Compares two derived values.
    friend bool operator==(Derived const&, Derived const&) { return true; }
};

/// Implementation-defined intermediate passing itself as the CRTP argument.
struct mid : equality_comparable<mid>
{
};

}

/// Derives from the implementation-defined intermediate.
struct leaf : detail::mid
{
};

}  // namespace lib
