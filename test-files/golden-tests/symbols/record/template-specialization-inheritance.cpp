template<int I, typename T = void>
struct S0
{
    struct S1 {};
};

template<>
struct S0<3>
{
};

template<>
struct S0<6>
{
};

// Terminal visitor should decay S0<1> to S0
struct R0 : S0<1> { };

// Terminal visitor should decay S0<2>::S1 to S0::S1
struct R1 : S0<2>::S1 { };

// Terminal visitor should NOT decay S0<3> to S0
struct R2 : S0<3> { };

// Terminal visitor should decay S0<4> to S0
using U1 = S0<4>;

// Terminal visitor should decay S0<5>::S1 to S0::S1
using U2 = S0<5>::S1;

// Terminal visitor should NOT decay S0<6> to S0
using U3 = S0<6>;

