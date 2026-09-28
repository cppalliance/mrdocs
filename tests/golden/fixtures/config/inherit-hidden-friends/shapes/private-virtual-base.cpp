// Given: three page-less bases. `private_derived : private private_base`,
//   whose friend `operator==(private_base, private_base)` takes the base;
//   `counted : private counter<counted>`, the Boost.Operators idiom, whose
//   CRTP friend `operator==(Derived const&, Derived const&)` takes the derived
//   type; and `virtual_derived : virtual virtual_base`, a public virtual base.
// Setting: `inherit-hidden-friends: copy-dependencies`; `lib::detail` is
//   implementation-defined; `extract-private-bases` so the private bases are
//   seen at all.
// Expect: nothing under `private_derived`, since calling the friend would need
//   a derived-to-base conversion that a private base makes inaccessible;
//   `lib::operator==(counted const&, counted const&)` under `counted`, whose
//   friend needs no conversion; and
//   `lib::operator==(virtual_derived, virtual_derived)` under `virtual_derived`.

namespace lib {

namespace detail {

/// Implementation-defined base whose friend takes the base itself.
struct private_base
{
    /// Compares two private-base values.
    friend bool operator==(private_base, private_base) { return true; }
};

/// Implementation-defined CRTP base whose friend takes the derived type.
template <class Derived>
struct counter
{
    /// Compares two derived values.
    friend bool operator==(Derived const&, Derived const&) { return true; }
};

/// Implementation-defined base inherited virtually.
struct virtual_base
{
    /// Compares two virtual-base values.
    friend bool operator==(virtual_base, virtual_base) { return true; }
};

}

/// Derives privately from a base whose friend takes the base.
struct private_derived : private detail::private_base
{
};

/// Derives privately from the CRTP base, as Boost.Operators is used.
struct counted : private detail::counter<counted>
{
};

/// Derives virtually and publicly.
struct virtual_derived : virtual detail::virtual_base
{
};

}  // namespace lib
