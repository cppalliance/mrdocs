struct A
{
    void f(int);
};

struct B
{
    void f(bool);
};

struct C : A, B
{
    using A::f;
    using B::f;
};