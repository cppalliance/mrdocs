[[noreturn]] void f1();

struct T
{
    [[noreturn]] static void f2();

    [[noreturn]] void f3();

    friend void f1();
};
