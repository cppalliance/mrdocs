// A derived class redeclaring base members with the same signature spelled
// differently: other parameter names and default arguments, top-level const
// on a by-value parameter, and an array parameter that decays to a pointer.
// None of these changes the signature, so each redeclaration is the same
// function as the base's and `derived` lists it once. A parameter whose
// type is a nested type of the same name in each class is a different
// function, so `r` is listed twice.

/// Base with members the derived class redeclares.
struct base
{
    /// Base f.
    void f(int x, int y = 0);

    /// Base g.
    void g(int x);

    /// Base h.
    void h(const int x);

    /// Base p.
    void p(int values[]);

    /// Base q, not redeclared: inherited as usual.
    void q(int x);

    /// A nested type of the base.
    struct X {};

    /// Base r, taking the base's `X`.
    void r(X x);
};

/// Redeclares `f`, `g`, `h`, and `p` with the same signatures.
struct derived : base
{
    /// Derived f.
    void f(int a, int b = 1);

    /// Derived g.
    void g(int value);

    /// Derived h.
    void h(int x);

    /// Derived p.
    void p(int* values);

    /// A nested type of the derived class, unrelated to the base's `X`.
    struct X {};

    /// Derived r, taking the derived class's `X`: a different function.
    void r(X x);
};
