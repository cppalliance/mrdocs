namespace lib {

/** A tool

    The only variable of this type matches `implementation-defined`,
    so the type stays documented.
*/
struct tool_t
{
    /** Use the tool

        @param x The value.
        @return The value.
    */
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

namespace detail::imag_impl {

struct imag_t
{
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

} // namespace detail::imag_impl

/** The imaginary part of a complex value

    @param z The value.
    @return The imaginary part.
*/
inline constexpr detail::imag_impl::imag_t imag{};

namespace detail {

template<int Precision>
struct convert_t
{
    constexpr double
    operator()(double x) const
    {
        return x;
    }
};

template<int N>
struct pick_t;

template<>
struct pick_t<0>
{
    template<typename T>
    constexpr double
    operator()(T const& x) const
    {
        return x;
    }
};

struct state_t
{
    int count;
};

struct norm_t
{
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

} // namespace detail

/** Convert a value to `double` with a given precision

    The type of this function object depends on the template parameter
    of the variable.

    @param x The value.
    @return The converted value.
*/
template<int Precision>
inline constexpr detail::convert_t<Precision> to_real{};

/** Convert a value to `double`

    The type of this function object is a specialization of an
    implementation-defined class template.

    @param x The value.
    @return The converted value.
*/
inline constexpr detail::convert_t<64> to_double{};

/** Pick a value

    The type of this function object is an explicit specialization of
    an implementation-defined class template.

    @param x The value.
    @return The value.
*/
inline constexpr detail::pick_t<0> pick{};

inline constexpr tool_t hidden_tool{};

/** Operations on complex values
*/
struct complex_ops
{
    /** The norm of a complex value

        @param z The value.
        @return The norm.
    */
    static constexpr detail::norm_t norm{};
};

/** A state

    Its implementation-defined type is not a function object, so it
    stays a variable.
*/
inline detail::state_t state{};

} // namespace lib
