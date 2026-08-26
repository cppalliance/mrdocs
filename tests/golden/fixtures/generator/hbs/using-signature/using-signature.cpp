// Render the signature of every form of using-declaration: with and
// without the `typename` keyword, off a concrete base and off a base
// that is a template parameter.

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
    /// A type, so the declaration says `typename`.
    using typename BaseT::size_type;

    /// A function.
    using BaseT::size;
};

/// Re-export the same members of a concrete base.
template <class T>
struct Concrete : private Base
{
    /// A type, so the declaration says `typename`.
    using typename Base::size_type;

    /// A function.
    using Base::size;
};
