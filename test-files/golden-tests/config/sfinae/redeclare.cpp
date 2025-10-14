// issue #850

#include <std.hpp>

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>);

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>);

template <typename T>
void f(std::enable_if_t<std::is_class_v<T>>)
{
}