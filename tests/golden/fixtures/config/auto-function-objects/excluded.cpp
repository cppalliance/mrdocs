namespace other {

struct sqrt_t
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

} // namespace other

namespace lib {

namespace detail {

struct norm_t
{
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

template<int Precision>
struct convert_t
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

template<int Factor>
struct scale_t
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return Factor * x;
    }
};

struct state_t
{
    int count;
};

template<typename T>
using identity = T;

} // namespace detail

/** The norm of a complex value

    Its type matches `exclude-symbols`.

    @param z The value.
    @return The norm.
*/
inline constexpr detail::norm_t norm{};

/** Convert a value to `double`

    Its type is a specialization of a class template that matches
    `exclude-symbols`.

    @param x The value.
    @return The converted value.
*/
inline constexpr detail::convert_t<64> to_double{};

extern "C++" {

/** Double a value

    Its type is a specialization of a class template that matches
    `exclude-symbols`, and it is declared in a linkage specification.

    @param x The value.
    @return The doubled value.
*/
inline constexpr detail::scale_t<2> twice{};

} // extern "C++"

/** The square root of a value

    Its type does not match `include-symbols`.

    @param x The value.
    @return The square root.
*/
inline constexpr other::sqrt_t sqrt{};

/** A state

    Its type matches `exclude-symbols` and is not a function object, so
    it stays a variable.
*/
inline detail::state_t state{};

/** The answer

    Its type is a specialization of an alias template that matches
    `exclude-symbols` and names no class, so it stays a variable.
*/
inline constexpr detail::identity<int> answer = 42;

} // namespace lib
