template<typename T>
struct A
{
    template<typename U>
    struct B
    {
        struct C { };

        template<typename V>
        struct D { };

        template<>
        struct D<bool> { };
    };

    template<>
    struct B<double>
    {
        struct C { };

        template<typename V>
        struct D { };

        template<>
        struct D<bool> { };
    };
};

template<>
template<typename U>
struct A<long>::B
{
    struct C { };

    template<typename V>
    struct D { };

    template<>
    struct D<bool> { };
};

template<>
template<>
struct A<short>::B<void>
{
    struct C { };

    template<typename V>
    struct D { };

    template<>
    struct D<bool> { };
};

struct E
{
    A<float>::B<double> m0;
    A<long>::B<double> m1;
    A<long>::B<float> m2;
    A<unsigned>::B<float> m3;
    A<short>::B<void> m4;

    A<float>::B<double>::C m5;
    A<long>::B<double>::C m6;
    A<long>::B<float>::C m7;
    A<unsigned>::B<float>::C m8;
    A<short>::B<void>::C m9;

    A<float>::B<double>::D<bool> m10;
    A<long>::B<double>::D<bool> m11;
    A<long>::B<float>::D<bool> m12;
    A<unsigned>::B<float>::D<bool> m13;
    A<short>::B<void>::D<bool> m14;
};
