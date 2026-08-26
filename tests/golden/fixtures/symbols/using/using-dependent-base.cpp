// Test that a using-declaration whose qualifier depends on a template
// parameter is extracted. Nothing is named until the template is
// instantiated, so the declaration records the name it introduces and
// no target. The same declarations off a concrete base are here for
// comparison, since those name something already.

/// A base with members to re-export.
struct Base
{
    /// The size type.
    using size_type = int;

    /// Return the size.
    size_type size() const;
};

/// Re-export members of a base that is a template parameter.
template <class T, class BaseT = Base>
struct Dependent : private BaseT
{
    /// A type re-exported from a dependent base.
    using typename BaseT::size_type;

    /// A function re-exported from a dependent base.
    using BaseT::size;

    /// A member declared directly, for comparison.
    int count() const;
};

/// Re-export the same members of a concrete base.
template <class T>
struct Concrete : private Base
{
    /// A type re-exported from a concrete base.
    using typename Base::size_type;

    /// A function re-exported from a concrete base.
    using Base::size;
};
