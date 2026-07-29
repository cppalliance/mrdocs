template<typename T>
struct A
{
    template<typename U>
    void f() { }

    template<>
    void f<int>() { }
};
