// A using-declaration that inherits constructors is written with the name
// of the base class, and that is the name it is documented under. Clang
// stores it as a constructor of the derived class instead, and, when the
// base depends on a template parameter, as a constructor of a type with
// no class behind it at all. Both shapes are here, along with a base that
// is a template parameter, a pack of them (the synopsis doesn't show the
// ellipsis yet), and a base named through an alias.

/// A base with constructors to inherit.
struct Base
{
    /// Construct an empty base.
    Base();

    /// Construct from a count.
    explicit Base(int);
};

/// Inherit the constructors of a concrete base.
struct Concrete : Base
{
    /// The constructors of `Base`.
    using Base::Base;
};

/// A base template with constructors to inherit.
template <class T>
struct BaseTemplate
{
    /// Construct an empty base.
    BaseTemplate();

    /// Construct from a value.
    explicit BaseTemplate(T);
};

/// Inherit the constructors of a base that depends on a template parameter.
template <class T>
struct Dependent : BaseTemplate<T>
{
    /// The constructors of `BaseTemplate`.
    using BaseTemplate<T>::BaseTemplate;
};

/// Inherit the constructors of a base that is a template parameter.
template <class B>
struct Mixin : B
{
    /// The constructors of `B`.
    using B::B;
};

/// Inherit the constructors of every base in a pack.
template <class... Bases>
struct FromPack : Bases...
{
    /// The constructors of each of `Bases`.
    using Bases::Bases...;
};

/// An alias of the base.
using BaseAlias = Base;

/// Inherit the constructors of a base named through an alias.
struct ViaAlias : Base
{
    /// The constructors of `BaseAlias`.
    using BaseAlias::BaseAlias;
};

/// Inherit the constructors of a base named through a member alias.
template <class T>
struct ViaMemberAlias : BaseTemplate<T>
{
    /// The base class.
    using base_type = BaseTemplate<T>;

    /// The constructors of `base_type`.
    using base_type::base_type;
};
