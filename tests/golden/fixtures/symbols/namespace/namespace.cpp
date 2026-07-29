namespace A
{
    void f0();

    namespace B
    {
        void f1();
    }

    inline namespace C
    {
        void f2();
    }

    namespace
    {
        void f3();
    }
}

inline namespace D
{
    void f5();

    namespace E
    {
        void f6();
    }

    inline namespace F
    {
        void f7();
    }

    namespace
    {
        void f8();
    }
}

namespace
{
    void f10();

    namespace G
    {
        void f11();
    }

    inline namespace H
    {
        void f12();
    }

#if 0
    namespace 
    {
        void f13();
    }
#endif
}

namespace I
{
    inline namespace
    {
        void f14();
    }
}