namespace N0
{
    void f0_WL();
    void f1_BL();
}

namespace N1
{
    void f0_BL();

    namespace N2
    {
        void f1_BL();
    }

    namespace N3_WL
    {
        void f1_WL();
    }

    namespace N4
    {
        void f1_WL();
        void f2_BL();
    }
}

namespace N5
{
    void f0();

    namespace N6_BL
    {
        void f1_BL();

        namespace N7
        {
            void f2_WL();
        }

        namespace M7
        {
            void f2_WL();
        }

        namespace N8
        {
            void f2_BL();
        }
    }
}
