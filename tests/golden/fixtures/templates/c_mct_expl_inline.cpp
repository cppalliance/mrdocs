struct A
{
    template<typename T>
    struct B { void f() { } };

    template<>
    struct B<int> { void g() { } };
};
