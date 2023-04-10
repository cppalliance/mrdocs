struct T {};

enum class E
{
    zero = 0,
    one = 1
};

struct U
{
    U();
    ~U();

    struct X {};

    using Y = X;

    void g1(X x);
    X g2();
    void g3(int(*f)());
    E g4();
    void g5(Y y);

protected:
    int f1();
    void f2(int a, char b, char const* c) noexcept;
    static void s3();
};
