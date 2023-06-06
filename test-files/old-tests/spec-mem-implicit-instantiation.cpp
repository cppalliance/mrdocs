template<typename T>
struct A
{
    void f() { }

    template<typename U>
    struct B
    {
        void g() { }
    };

    template<typename U>
    struct C
    {
        void h() { }
    };
};

template<>
void A<int>::f() { }

template<>
template<>
void A<int>::B<long>::g() { }

template<>
template<typename U>
struct A<short>::C 
{
    void i() { }
};


template<>
template<typename U>
struct A<bool>::C<U*>
{
    void j() { }
};

template<>
template<>
void A<bool>::C<double*>::j() { }
