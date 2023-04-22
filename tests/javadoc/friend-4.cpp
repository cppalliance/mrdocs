struct T
{
    friend void f();
};

struct U
{
    /// U::f
    friend void f();
};

void f();
