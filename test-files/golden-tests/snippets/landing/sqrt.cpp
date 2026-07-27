#include <type_traits>

// tag::doc[]
struct sqrt_fn
{
    /** Compute the integer square root.

        Returns \f$\lfloor\sqrt{value}\rfloor\f$, the largest integer whose
        square does not exceed `value`, computed via bit manipulation.

        @par Complexity
        Logarithmic in `value`, about $O(\log value)$ iterations.

        @note Returns zero for zero input.

        @tparam T An integral type.
        @param value The integral value, which must be non-negative.
        @pre `value >= 0`.
        @return The integer square root of `value`.
    */
    template <typename T>
    [[nodiscard]] constexpr
    std::enable_if_t<std::is_integral_v<T>, T>
    operator()(T value) const noexcept;
};

constexpr sqrt_fn sqrt = {};
// end::doc[]
