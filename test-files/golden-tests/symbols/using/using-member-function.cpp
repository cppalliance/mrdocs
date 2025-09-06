// This test creates various classes with a member function `f`
// and then uses `using` declarations to bring them into a derived class,
// where these functions will form an overload set.
// The way the relationship takes place depends on how the base classes
// are defined, and how the classes are inherited.

/// A tag template to create distinct f functions.
template <int idx>
struct Tag {};

/// This struct will be inherited as public
struct A
{
    /** Public member function f taking a Tag<0>.

        That is the only member function that will
        be publicly accessible in U via inheritance.

     */
    void f(Tag<0>);
};

/// This struct will be inherited as public
struct B
{
protected:
    /// Protected member function f taking a Tag<1>.
    void f(Tag<1>);
};

/// This struct will be inherited as protected
struct C
{
    /// Public member function f taking a Tag<2>.
    void f(Tag<2>);
};

/// This struct will be inherited as protected
struct D
{
protected:
    /// Protected member function f taking a Tag<3>.
    void f(Tag<3>);
};

/// This struct will be inherited as private
struct E
{
    /// Public member function f taking a Tag<4>.
    void f(Tag<4>);
};

/// This struct will be inherited as private
struct F
{
protected:
    /// Protected member function f taking a Tag<5>.
    void f(Tag<5>);
};

/** This struct inherits from A, B, C, D, E, and F in various ways,

    The documentation of this struct should include the function
    f twice: once in the member functions

 */
struct U :
    public A,
    public B,
    protected C,
    protected D,
    private E,
    private F
{
    /// Bring all the A::A constructors into U.
    using A::A;
    /// Bring all the B::B constructors into U.
    using B::B;
    /// Bring all the C::C constructors into U.
    using C::C;
    /// Bring all the D::D constructors into U.
    using D::D;
    /// Bring all the E::E constructors into U.
    using E::E;
    /// Bring all the F::F constructors into U.
    using F::F;

    /// Bring all the A::f functions into U.
    using A::f;
    /// Bring all the B::f functions into U.
    using B::f;
    /// Bring all the C::f functions into U.
    using C::f;
    /// Bring all the D::f functions into U.
    using D::f;
    /// Bring all the E::f functions into U.
    using E::f;
    /// Bring all the F::f functions into U.
    using F::f;
};
