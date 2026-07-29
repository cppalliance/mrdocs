class A
{
    friend class B;
    friend class B;
    friend class B;

    friend void f();
    friend void f();
    friend void f();
};

class B {};

void f();