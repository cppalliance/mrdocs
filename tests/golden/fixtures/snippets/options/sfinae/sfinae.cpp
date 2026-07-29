#include <type_traits>

/** Multiply by two, but only for integers.

    The `std::enable_if_t` SFINAE on the return type
    restricts the overload to integral arguments.
*/
template <class T>
std::enable_if_t<std::is_integral_v<T>, T>
twice(T x);
