namespace N0
{
    void f0();

    namespace N1_BL
    {
        void f1();
    }

    struct S0_BL
    {
        void f2();
    };
}

void N0::f0();
void N0::N1_BL::f1();
void N0::S0_BL::f2() { }

namespace N2_BL
{
    void f0_BL();

    namespace N3_BL
    {
        void f1();
    }

    void f2();
}

namespace N3
{
    void f0_BL();
    void f1_BL();
}

namespace N4
{
    namespace N5
    {
        void f0_BL();
        void f1();
    }

    namespace N6
    {
        void f0_BL();
        void f1();
    }
}

namespace N7
{
    namespace N8
    {
        void f0_BL();
        void g0_BL();
    }

    namespace N9
    {
        void f0_BL();
        void g0();
    }
}

namespace N10
{
    void f0_BL();
    namespace N11
    {
        void f1_BL();
    }
}

namespace N12
{
    void f0_BL();
    namespace N13
    {
        void f1_BL();
        namespace N14
        {
            void f2_BL();
        }
    }
}
