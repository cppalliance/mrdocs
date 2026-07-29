template<int I, typename T = void>
struct S0
{
    void f0() { }

    struct S1
    {
        void f1() { }

        template<int J, typename U = void>
        struct S2
        {
            void f2();

            struct S3
            {
                void f3();
            };

            template<int K, typename V = void>
            struct S4
            {
                void f4();
            };
        };
    };

    template<int J, typename U = void>
    struct S5
    {
        void f5();

        struct S6
        {
            void f6() { }

            template<int K, typename V = void>
            struct S7
            {
                void f7();

                struct S8
                {
                    void f8();
                };

                template<int L, typename W = void>
                struct S9
                {
                    void f9();
                };
            };
        };
    };
};

template<>
struct S0<0> { };

template<typename T>
struct S0<1, T*> { };

template<>
struct S0<1, int*> { };

template<>
struct S0<2>::S1 { };

template<>
template<int I, typename T>
struct S0<3>::S1::S2 { };

template<>
template<>
struct S0<4>::S1::S2<5> { };

template<>
template<typename T>
struct S0<6>::S1::S2<7, T*> { };

template<>
template<>
struct S0<6>::S1::S2<7, int*> { };

template<>
template<>
struct S0<8>::S1::S2<9>::S3 { };

template<>
template<>
template<int I, typename T>
struct S0<10>::S1::S2<11>::S4 { };

template<>
template<>
template<>
struct S0<12>::S1::S2<13>::S4<14> { };

template<>
template<>
template<typename T>
struct S0<15>::S1::S2<16>::S4<17, T*> { };

template<>
template<>
template<>
struct S0<15>::S1::S2<16>::S4<17, int*> { };

template<>
template<int I, typename T>
struct S0<18>::S5 { };

template<>
template<>
struct S0<19>::S5<20> { };

template<>
template<typename T>
struct S0<21>::S5<22, T*> { };

template<>
template<>
struct S0<21>::S5<22, int*> { };

template<>
template<>
struct S0<23>::S5<24>::S6 { };

template<>
template<>
template<int I, typename T>
struct S0<25>::S5<26>::S6::S7 { };

template<>
template<>
template<typename T>
struct S0<27>::S5<28>::S6::S7<29, T*> { };

template<>
template<>
template<>
struct S0<27>::S5<28>::S6::S7<29, int*> { };

template<>
template<>
template<>
struct S0<30>::S5<31>::S6::S7<32> { };

template<>
template<>
template<>
struct S0<33>::S5<34>::S6::S7<35>::S8 { };

template<>
template<>
template<>
template<int I, typename T>
struct S0<36>::S5<37>::S6::S7<38>::S9 { };

template<>
template<>
template<>
template<typename T>
struct S0<39>::S5<40>::S6::S7<41>::S9<42, T*> { };

template<>
template<>
template<>
template<>
struct S0<39>::S5<40>::S6::S7<41>::S9<42, int*> { };

template<>
template<>
template<>
template<>
struct S0<43>::S5<44>::S6::S7<45>::S9<46> { };

struct R0 : S0<-1> { };

struct R1 : S0<0> { };

struct R2 : S0<1, void*> { };

struct R3 : S0<1, int*> { };

struct R4 : S0<2>::S1 { };

struct R5 : S0<3>::S1::S2<-1> { };

struct R6 : S0<4>::S1::S2<5> { };

struct R7 : S0<6>::S1::S2<7, void*> { };

struct R8 : S0<6>::S1::S2<7, int*> { };

struct R9 : S0<8>::S1::S2<9>::S3 { };

struct R10 : S0<10>::S1::S2<11>::S4<-1> { };

struct R11 : S0<12>::S1::S2<13>::S4<14> { };

struct R12 : S0<15>::S1::S2<16>::S4<17, void*> { };

struct R13 : S0<15>::S1::S2<16>::S4<17, int*> { };

struct R14 : S0<18>::S5<-1> { };

struct R15 : S0<19>::S5<20> { };

struct R16 : S0<21>::S5<22, void*> { };

struct R17 : S0<21>::S5<22, int*> { };

struct R18 : S0<23>::S5<24>::S6 { };

struct R19 : S0<25>::S5<26>::S6::S7<-1> { };

struct R20 : S0<27>::S5<28>::S6::S7<29, void*> { };

struct R21 : S0<27>::S5<28>::S6::S7<29, int*> { };

struct R22 : S0<30>::S5<31>::S6::S7<32> { };

struct R23 : S0<33>::S5<34>::S6::S7<35>::S8 { };

struct R24 : S0<36>::S5<37>::S6::S7<38>::S9<-1> { };

struct R25 : S0<39>::S5<40>::S6::S7<41>::S9<42, void*> { };

struct R26 : S0<39>::S5<40>::S6::S7<41>::S9<42, int*> { };

struct R27 : S0<43>::S5<44>::S6::S7<45>::S9<46> { };

struct R28 : S0<0, bool> { };

struct R29 : S0<1, int> { };

struct R30 : S0<2, bool>::S1 { };

template<int I, typename T>
struct R31 : S0<3, bool>::S1::S2<I, T> { };

struct R32 : S0<4, bool>::S1::S2<5, bool> { };

struct R33 : S0<6, bool>::S1::S2<7, int> { };

struct R34 : S0<8, bool>::S1::S2<9, bool>::S3 { };

template<int I, typename T>
struct R35 : S0<10, bool>::S1::S2<11, bool>::S4<I, T> { };

struct R36 : S0<12, bool>::S1::S2<13, bool>::S4<14, bool> { };

struct R37 : S0<15, bool>::S1::S2<16, bool>::S4<17, int> { };

template<int I, typename T>
struct R38 : S0<18, bool>::S5<I, T> { };

struct R39 : S0<19, bool>::S5<20, bool> { };

struct R40 : S0<21, bool>::S5<22, int> { };

struct R41 : S0<23, bool>::S5<24, bool>::S6 { };

template<int I, typename T>
struct R42 : S0<25, bool>::S5<26, bool>::S6::S7<I, T> { };

struct R43 : S0<27, bool>::S5<28, bool>::S6::S7<29, int> { };

struct R44 : S0<30, bool>::S5<31, bool>::S6::S7<32, bool> { };

struct R45 : S0<33, bool>::S5<34, bool>::S6::S7<35, bool>::S8 { };

template<int I, typename T>
struct R46 : S0<36, bool>::S5<37, bool>::S6::S7<38, bool>::S9<I, T> { };

struct R47 : S0<39, bool>::S5<40, bool>::S6::S7<41, bool>::S9<42, int> { };

struct R48 : S0<43, bool>::S5<44, bool>::S6::S7<45, bool>::S9<46, bool> { };
