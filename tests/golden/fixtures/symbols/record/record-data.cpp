// data members

struct T
{
    int i;
    double j;
    mutable int k;
    int l : 8;
    int m : 4 + 4;
    int : 1;
    int : 0;
};

struct U
{
    T t;
};

class V
{
    int i;
protected:
    unsigned long j;
private:
    double k;
};

struct W
{
    char buf[64];
};

template<typename P, int I>
struct X
{
    using Q = P;

    int x0 = 0;
    P x1;
    const P x2[32];
    Q x3;

    int x4 : I + 4;
};
