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
    // Explore different behaviors when using declarations from
    // multiple base classes
    // Constructors and destructors are inherited as "C" for the user

    using A::f;
    using B::f;
};
