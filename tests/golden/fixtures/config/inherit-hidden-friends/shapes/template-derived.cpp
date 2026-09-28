// Given: the mp-units shape, `template <class T> struct quantity :
//   detail::quantity_ops`, where the non-template base declares the hidden
//   friend `operator==(quantity_ops const&, quantity_ops const&)`.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined.
// Expect: `lib::operator==(quantity const&, quantity const&)` listed under
//   `quantity`'s Non-Member Functions. The operand is spelled `quantity`, not
//   `quantity<T>`: substitution names the class template, not a specialization.

namespace lib {

namespace detail {

/// Implementation-defined base carrying the operators of every `quantity`.
struct quantity_ops
{
    /// Compares two quantities.
    friend bool operator==(quantity_ops const&, quantity_ops const&) { return true; }
};

}

/// A class template deriving from the operators base.
template <class T>
struct quantity : detail::quantity_ops
{
};

}  // namespace lib
