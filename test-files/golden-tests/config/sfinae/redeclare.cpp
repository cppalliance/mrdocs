// issue #850

namespace std
{
template <bool C, typename T = void>
struct enable_if
{
    using type = T;
};
template <typename T>
struct enable_if<false, T>
{};

template <bool C, typename T = void>
using enable_if_t = typename enable_if<C, T>::type;

template <typename T>
struct is_class
{
    static constexpr bool value = true;
};

template <typename T>
bool is_class_v = is_class<T>::value;
}

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>);

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>);

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>)
{
}