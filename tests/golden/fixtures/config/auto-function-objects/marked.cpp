namespace lib {

namespace detail {

struct imag_t
{
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

/** The type of `real`

    @functionobject
*/
struct real_t
{
    /** The real part of a complex value

        @param z The value.
        @return The real part.
    */
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

} // namespace detail

namespace excluded {

struct abs_t
{
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

/** The type of `norm`

    @functionobject
*/
struct norm_t
{
    /** The norm of a complex value

        @param z The value.
        @return The norm.
    */
    template<typename T>
    constexpr double
    operator()(T const& z) const
    {
        return 0;
    }
};

} // namespace excluded

/** The imaginary part of a complex value

    With `auto-function-objects` off, the command alone documents this
    function object as a function, though its type is
    implementation-defined.

    @functionobject
    @param z The value.
    @return The imaginary part.
*/
inline constexpr detail::imag_t imag{};

/** The real part of a complex value

    The command is on the implementation-defined type.
*/
inline constexpr detail::real_t real{};

/** The absolute value of a complex value

    The type matches `exclude-symbols`.

    @functionobject
    @param z The value.
    @return The absolute value.
*/
inline constexpr excluded::abs_t abs{};

/** The norm of a complex value

    The command is on the type, which matches `exclude-symbols`.
*/
inline constexpr excluded::norm_t norm{};

} // namespace lib
