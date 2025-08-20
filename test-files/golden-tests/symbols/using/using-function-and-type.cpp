namespace A
{
    void f(int);
    struct f {};
}

// This using declaration will shadow both the function and the type.
using A::f;

struct f a;

void
g()
{
    f(2);
}


