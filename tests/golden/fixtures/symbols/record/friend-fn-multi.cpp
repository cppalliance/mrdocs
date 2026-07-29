struct T
{
    /// T::f
    friend void f();
};

struct U
{
    friend void f();
};

void f();
