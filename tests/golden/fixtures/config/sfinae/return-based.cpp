// issue #849

#include <type_traits>

template <typename T>
std::enable_if_t<std::is_class_v<T>, int> f();
