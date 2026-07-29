template<typename T>
struct A
{
    template<typename U, typename V>
    constexpr static T x = 0;

    template<typename U>
    constexpr static T x<U*, T> = 1;

    template<>
    constexpr static bool x<T, long> = 2;
};
