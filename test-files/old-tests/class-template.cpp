struct S { };

template<typename T>
struct C0
{
    int w;
    S x;
    T y;

    struct N0
    {
        int z;
    };
};

struct C1
{
    int w;

    template<typename T>
    struct N1
    {
        S x;
        T y;
        int z;
    };
};

template<typename T>
struct C2
{
    int v;

    template<typename U>
    struct N2
    {
        S w;
        T x;
        U y;
        int z;
    };
};

template<typename T>
struct C3;

template<typename T>
struct C3
{
    int v;
};
