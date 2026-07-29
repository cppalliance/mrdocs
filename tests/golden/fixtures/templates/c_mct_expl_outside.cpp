struct A
{
    template<typename T>
    struct B { void f() { } };

};

template<>
struct A::B<int> { void g() { } };
