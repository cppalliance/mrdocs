#include <type_traits>

/** Computes the integer square root \f$\lfloor\sqrt{value}\rfloor\f$.

    This function calculates the square root of a given integral value
    using bit manipulation, in $O(\log value)$ time.

    @throws std::invalid_argument if the input value is negative.

    @tparam T The type of the input value. Must be an integral type.
    @param value The integral value to compute the square root of.
    @return The square root of the input value.
 */
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T> sqrt(T value);
