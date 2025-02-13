struct A {
    void f();
    int f(int);

    static void g();
    static int g(int);

    friend bool operator==(A, A);
    friend bool operator==(A, int);
};

void f();
int f(int);

struct B {};

bool operator==(B, B);
bool operator==(B, int);

