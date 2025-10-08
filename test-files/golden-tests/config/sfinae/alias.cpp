#include <type_traits>

// just like std::enable_if_t
template <bool C, typename T>
using enable_if_t = typename std::enable_if<C, T>::type;

template <typename T>
enable_if_t<std::is_class_v<T>, T> f1();

// reversed param order
template <typename T, bool C>
using if_enable_t = typename std::enable_if<C, T>::type;

template <typename T>
if_enable_t<T, std::is_class_v<T>> f2();
