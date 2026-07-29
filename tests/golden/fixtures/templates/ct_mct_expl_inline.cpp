template<typename T>
struct A
{
    template<typename U>
    struct B { void f() { } };

    template<>
    struct B<int> { void g() { } };
};
