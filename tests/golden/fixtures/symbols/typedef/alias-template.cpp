template<typename T>
struct A { };

template<typename T, typename U>
struct B { };

template<typename T>
using C = A<T>;

template<typename T>
struct D
{
    template<typename U>
    using E = B<T, U>;
};
