template<typename T>
struct S0
{
    template<typename U>
    struct S2
    {
        using M2 = U;

        template<typename V>
        using M3 = S2<U>;
    };
};

using A5 = S0<void>;
using A9 = A5::S2<char>::M3<signed>::M3<unsigned>::M2;
