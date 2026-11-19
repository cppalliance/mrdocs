// A class that inherits the constructors of its base has a member named
// after the base: the using-declaration. A reference to that name from
// the class or from one of its members still means the base.

/// A base with constructors to inherit.
struct Base
{
    /// Construct from a count.
    explicit Base(int);
};

/// Inherit the constructors of @ref Base.
struct Derived : Base
{
    /// The constructors of `Base`.
    using Base::Base;

    /// @copydoc Base
    void f();
};

/// A base template with constructors to inherit.
template <class T>
struct BaseTemplate
{
    /// Construct from a value.
    explicit BaseTemplate(T);
};

/// Inherit the constructors of @ref BaseTemplate.
template <class T>
struct DependentDerived : BaseTemplate<T>
{
    /// The constructors of `BaseTemplate`.
    using BaseTemplate<T>::BaseTemplate;

    /// @copydoc BaseTemplate
    void g();
};

/// Inherit the constructors of a base named through a member alias.
template <class T>
struct AliasDerived : BaseTemplate<T>
{
    /// The base class.
    using base_type = BaseTemplate<T>;

    /// The constructors of `base_type`.
    using base_type::base_type;

    /// @copydoc base_type
    void h();
};
