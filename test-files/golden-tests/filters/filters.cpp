namespace A
{
    void f0();

    namespace detail
    {
        void f1();
        void g0();

        struct X
        {
            void f2();
        };

        struct Y
        {
            void f3();

            struct Z
            {
                void f8();
            };
        };
    }

    namespace detair
    {
        void f4();

        struct X
        {
            void f5();
        };

        struct Y
        {
            void f6();

            struct Z
            {
                void f7();
            };
        };
    }
}

namespace B
{
    void f0();

    namespace detail
    {
        void f1();
        void g0();

        struct X
        {
            void f2();
        };

        struct Y
        {
            void f3();

            struct Z
            {
                void f8();
            };
        };
    }

    namespace detair
    {
        void f4();

        struct X
        {
            void f5();
        };

        struct Y
        {
            void f6();

            struct Z
            {
                void f7();
            };
        };
    }
}


namespace C
{
    void f0();
    void g0();
}

namespace D
{
    namespace E
    {
        void f0();
        void g0();
    }
    void f1();
    void g1();
}


void D::E::f0();
void D::E::g0();


namespace F
{
    void f0();
    void g0();

    namespace G
    {
        void f1();
        void g1();
    }
}
