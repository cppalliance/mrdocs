namespace A
{
    /// A function
    void f(int);

    /// A record
    struct f {};
}

/// A using declaration that shadows a function and the record.
using A::f;
