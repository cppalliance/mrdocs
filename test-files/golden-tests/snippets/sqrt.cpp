#include <type_traits>

/** Computes the square root of an integral value.

    This function calculates the square root of a
    given integral value using bit manipulation.

    @throws std::invalid_argument if the input value is negative.

    @tparam T The type of the input value. Must be an integral type.
    @param value The integral value to compute the square root of.
    @return The square root of the input value.
 */
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T> sqrt(T value) {
    if (value < 0) {
        throw std::invalid_argument(
            "Cannot compute square root of a negative number");
    }
    T result = 0;
    // The second-to-top bit is set
    T bit = 1 << (sizeof(T) * 8 - 2);
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result += bit << 1;
        }
        result >>= 1;
        bit >>= 2;
    }
    return result;
}
