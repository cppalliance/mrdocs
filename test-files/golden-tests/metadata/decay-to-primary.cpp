template<typename T>
struct S0
{
    using M0 = T;

    template<typename U>
    using M1 = S0<U>;
};

using A0 = S0<void>;
using A1 = A0::M1<short>::M0;
