template<typename T>
struct A
{
    static int v0;
    static int v1;
    static const int v2;
    static const int v3;
    static const int v4;

    static inline int v5 = 5;
    static inline const int v6 = 6;
    static constexpr int v7 = 7;
};

struct B
{
    static thread_local const int x0 = 0;
    static constexpr thread_local int x1 = 0;
};

template<typename T>
int A<T>::v0 = 0;

template<typename T>
inline int A<T>::v1 = 1;

template<typename T>
constexpr int A<T>::v2 = 2;

template<typename T>
inline const int A<T>::v3 = 3;

template<typename T>
const int A<T>::v4 = 4;

auto f()
{
    return
        A<int>::v0 +
        A<int>::v1 +
        A<int>::v2 +
        A<int>::v3 +
        A<int>::v4 +
        A<int>::v5 +
        A<int>::v6 +
        A<int>::v7;
}

thread_local const int B::x0;
