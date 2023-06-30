template<typename T>
struct S0
{
    using M0 = T;

    struct S1 { };

    template<typename U>
    using M1 = S0<U>;

    template<typename U>
    struct S2
    {
        using M2 = U;

        struct S3 { };

        template<typename V>
        using M3 = S2<U>;

        template<typename V>
        struct S4
        {
            using M4 = V;
        };
    };
};

using A0 = S0<int>;
using A1 = A0::M0;
using A2 = A0::S1;
using A3 = S0<long>::M0;
using A4 = S0<long long>::S1;

using A5 = S0<void>;
using A6 = A5::M1<short>::M0;
using A7 = A5::S2<bool>::M2;
using A8 = A5::S2<int>::S3;
using A9 = A5::S2<char>::M3<signed>::M3<unsigned>::M2;
using A10 = A5::S2<float>::M3<double>::M3<long double>::S4<void>::M4;
