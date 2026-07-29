struct A
{
    template<typename T>
    void f() { }
};

template<>
void A::f<int>() { }
