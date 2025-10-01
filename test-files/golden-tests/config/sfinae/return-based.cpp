// issue #849

#include <std.hpp>

template <typename T>
std::enable_if_t<std::is_class_v<T>, int> f();
