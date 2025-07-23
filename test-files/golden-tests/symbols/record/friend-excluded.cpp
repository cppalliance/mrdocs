namespace detail {
class impl;
}

class B {};

void f();

class A
{
    friend class detail::impl;
    friend class B;
    friend void f();
};
