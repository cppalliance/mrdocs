void f0();

namespace A
{

    /**
        See @ref f0

        See @ref ::f0
    */
    void f1();

    /**
        See @ref f1

        See @ref A::f1

        See @ref ::A::f1
    */
    template<typename>
    struct B
    {
        void f2();
    };

    struct C
    {
        void f3();
        void f4();
    };

    struct D : C
    {
        void f4();

        /**
            See @ref f3

            See @ref f4

            See @ref C::f4
        */
        struct E { };
    };
}

/**
    See @ref A::f1

    See @ref ::A::f1
*/
void f5();

struct F
{
    void operator~();
    void operator,(F&);
    void operator()(F&);
    void operator[](F&);
    void operator+(F&);
    void operator++();
    void operator+=(F&);
    void operator&(F&);
    void operator&&(F&);
    void operator&=(F&);
    void operator|(F&);
    void operator||(F&);
    void operator|=(F&);
    void operator-(F&);
    void operator--();
    void operator-=(F&);
    void operator->();
    void operator->*(F&);
    void operator<(F&);
    void operator<<(F&);
    void operator<<=(F&);
    void operator<=(F&);
    void operator<=>(F&);
    void operator>(F&);
    void operator>>(F&);
    void operator>>=(F&);
    void operator>=(F&);
    void operator*(F&);
    void operator*=(F&);
    void operator%(F&);
    void operator%=(F&);
    void operator/(F&);
    void operator/=(F&);
    void operator^(F&);
    void operator^=(F&);
    void operator=(F&);
    void operator==(F&);
    void operator!();
    void operator!=(F&);
};

/**
    See @ref F::operator~

    See @ref F::operator,

    See @ref F::operator()

    See @ref F::operator[]

    See @ref F::operator+

    See @ref F::operator++

    See @ref F::operator+=

    See @ref F::operator&

    See @ref F::operator&&

    See @ref F::operator&=

    See @ref F::operator|

    See @ref F::operator||

    See @ref F::operator|=

    See @ref F::operator-

    See @ref F::operator--

    See @ref F::operator-=

    See @ref F::operator->

    See @ref F::operator->*

    See @ref F::operator<

    See @ref F::operator<<

    See @ref F::operator<<=

    See @ref F::operator<=

    See @ref F::operator<=>

    See @ref F::operator>

    See @ref F::operator>>

    See @ref F::operator>>=

    See @ref F::operator>=

    See @ref F::operator*

    See @ref F::operator*=

    See @ref F::operator%

    See @ref F::operator%=

    See @ref F::operator/

    See @ref F::operator/=

    See @ref F::operator^

    See @ref F::operator^=

    See @ref F::operator=

    See @ref F::operator==

    See @ref F::operator!

    See @ref F::operator!=
*/
void f6();
