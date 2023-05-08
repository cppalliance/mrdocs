template<typename T>
struct A
{
    template<typename U>
    void f() { }
};

template<>
template<>
void A<int>::f<int>() { }
