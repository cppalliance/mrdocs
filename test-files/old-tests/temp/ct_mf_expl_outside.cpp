template<typename T>
struct A
{
    void f() { }
};

template<>
void A<int>::f() { }
