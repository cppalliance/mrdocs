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
struct is_integral
{
    static constexpr bool value = true;
};

template <typename T>
bool is_integral_v = is_integral<T>::value;
}

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
