namespace lib {

namespace detail {

struct abs_t
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

} // namespace detail

namespace impl {

struct sqrt_t
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

} // namespace impl

/** The absolute value of a value

    Its type matches `see-below`.

    @param x The value.
    @return The absolute value.
*/
inline constexpr detail::abs_t abs{};

/** The square root of a value

    The variable matches `see-below` and its type is
    implementation-defined.

    @param x The value.
    @return The square root.
*/
inline constexpr impl::sqrt_t sqrt{};

} // namespace lib
