template<typename T>
struct A { };

template<typename T>
struct A<T*>
{
    template<typename U>
    struct B { };

    template<typename U>
    struct B<U*> { struct C { }; };

    template<>
    struct B<int> { };
};

template<>
struct A<double>
{
    template<typename U>
    struct D
    {
        template<typename T>
        struct E { };

        template<typename T>
        struct E<T*>
        {
            struct F { };
        };
    };

    template<>
    struct D<float>
    {
        template<typename T>
        struct G { };
    };
};

template<>
template<>
struct A<long*>::B<int*>::C { };

template<typename T>
struct A<double>::D<float>::G<T*> { };

template<>
template<>
struct A<double>::D<short>::E<int*>::F { };
