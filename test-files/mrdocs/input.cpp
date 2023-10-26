// canonicalization
namespace canonicalization {
struct B {};
struct b {};
struct A {};
struct a {};
struct Ba {};
struct bA {};
struct Bx {};
struct ba {};
}

// attributes
namespace attributes {
[[nodiscard]] bool f();
}

// access specifiers
namespace access {
struct S0
{
    void f0();
protected:
    void f1();
private:
    void f2();
};

class C0
{
    void f0();
protected:
    void f1();
public:
    void f2();
};

union U0
{
    void f0();
protected:
    void f1();
private:
    void f2();
};
}

// data members
namespace data {
struct T
{
    int i;
    double j;
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
}

// inheritance
namespace inheritance {
struct S0 {};
class C0 {};
union U0 {};

struct S1;
struct S1 {};

struct S2 : S0 {};
struct S3 : S1 {};
struct S4 : S2, S3 {};

class C1 : C0 {};
class C2 : public C0 {};
class C3 : protected C0 {};
class C4 : private C0 {};

class C5 : virtual C0 {};
class C6 : virtual C1 {};
class C7 : public C5, public C6 {};

struct S5 : private S0, protected S1 {};
}
