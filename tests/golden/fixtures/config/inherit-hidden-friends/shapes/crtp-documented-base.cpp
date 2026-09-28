// Given: `derived : crtp_base<derived>`, the same shape as
//   crtp-implementation-defined-base, but with `crtp_base` documented.
// Setting: `inherit-hidden-friends: copy-all`, nothing filtered.
// Expect: no copy. An operand naming a specialization whose primary has a
//   page keeps that spelling, so the substituted copy would read
//   `operator==(crtp_base<derived>, crtp_base<derived>)`, identical to the
//   documented friend of the instantiation; `derived` references that friend
//   instead of duplicating it.

namespace lib {

/// Documented CRTP base parameterized on the derived type.
template <class Derived>
struct crtp_base
{
    /// Compares two derived values.
    friend bool operator==(crtp_base, crtp_base) { return true; }
};

/// Concrete CRTP derived class.
struct derived : crtp_base<derived>
{
};

}  // namespace lib
