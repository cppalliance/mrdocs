template<typename T>
struct A
{
    struct B { void f() { } };
};

template<>
struct A<int>::B { void g() { } };
