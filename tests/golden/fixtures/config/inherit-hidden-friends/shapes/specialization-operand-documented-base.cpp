// Given: `derived : base<int>`, the same shape as specialization-operand,
//   but with `base` documented.
// Setting: `inherit-hidden-friends: copy-all`, nothing filtered.
// Expect: no copy. The `base<int>` operand keeps its spelling because the
//   primary `base` has a page, so the copy would be identical to the
//   documented `operator==(base<int>, base<int>)` of the instantiation;
//   `derived` references that friend instead.

namespace lib {

/// Documented base template whose friend takes the specialization.
template <class T>
struct base
{
    /// Compares two base values.
    friend bool operator==(base<T>, base<T>) { return true; }
};

/// Derives from a specialization of the documented base.
struct derived : base<int>
{
};

}  // namespace lib
