/// Issue #1172.
template<typename T, typename U = void, typename V = void>
struct A;

template<typename T, typename U, typename V>
struct A
{};

template<typename T, unsigned long N>
struct A<T[N], void, void>
{};
