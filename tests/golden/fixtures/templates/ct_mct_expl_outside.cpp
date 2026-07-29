template<typename T>
struct A
{
    template<typename U>
    struct B { void f() { } };
};

template<>
template<>
struct A<int>::B<int> { void g() { } };
