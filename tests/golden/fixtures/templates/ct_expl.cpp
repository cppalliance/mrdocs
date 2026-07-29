template<typename T>
struct A { void f() { } };

template<>
struct A<int> { void g() { } };
