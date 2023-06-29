// data members

struct T
{
    int i;
    double j;
    mutable int k;
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

template<typename P>
struct X
{
    using Q = P;

    int x0 = 0;
    P x1;
    const P x2[32];
    Q x3;
};
