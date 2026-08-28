/** A page at the top of the output.

    It refers to @ref other::socket, @ref geom::circle and
    @ref ns::response_factory.
*/
struct top
{
};

namespace outer {
namespace inner {

/** A page two directories down.

    It refers to @ref other::socket, @ref geom::circle and
    @ref ns::response_factory.

    It also uses @ref odd::thing::get and @ref odd::thing::quote, whose
    locations need escaping.
*/
struct nested
{
};

} // namespace inner
} // namespace outer
