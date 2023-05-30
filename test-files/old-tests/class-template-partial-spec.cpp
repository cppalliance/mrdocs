template<typename T>
struct A
{
    template<typename U, typename V>
    struct B { };

    template<typename U>
    struct B<U*, T> { };

    template<>
    struct B<T, long> { };
};
