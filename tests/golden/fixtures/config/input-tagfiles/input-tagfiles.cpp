/** A socket wrapper this corpus documents.

    Wraps an @ref other::socket, which another documentation set covers,
    and is closed with @ref other::socket::close. One is opened by
    @ref other::connect, which that set documents on the page of the
    namespace holding it, while @ref elsewhere is a namespace of it with
    a page of its own.

    @ref socket stays plain text, and so does @ref other::missing.
*/
struct wrapper
{
};

namespace other {

/** A class of ours, in a namespace another set also documents.

    From in here the names need no qualifying, exactly as they would not
    for a symbol of this corpus: @ref socket, and its @ref socket::close.
*/
struct local
{
};

} // namespace other
