struct A
{
    template<typename T>
    void f() { }

    template<>
    void f<int>() { }
};
