namespace N 
{
    template<typename T>
    struct A { };
    
    template<typename T>
    using B = A<T>;
    
    template<typename T>
    using C = B<T>;

    struct D { };
}

struct E : N::C<int> { };
