/** A socket wrapper this corpus documents.

    Wraps an @ref other::socket, which another documentation set covers,
    and is closed with @ref other::socket::close. One is opened by
    @ref other::connect, which that set documents on the page of the
    namespace holding it, while @ref elsewhere is a namespace of it with
    a page of its own.

    Written as a call, @ref other::connect() links just the same, while
    @ref other names the namespace this corpus documents, not that set's.

    @ref socket stays plain text, and so does @ref other::missing.
*/
struct wrapper
{
};

/** Open the connection a wrapper holds.

    @param w The wrapper to open.
    @throws other::socket_error The connection cannot be made.
*/
void
open(wrapper& w);

namespace other {

/** A class of ours, in a namespace another set also documents.

    From in here the names need no qualifying, exactly as they would not
    for a symbol of this corpus: @ref socket, and its @ref socket::close.
    Written with a leading `::`, though, @ref ::socket names a socket in
    the global namespace, which no set documents, so it stays plain text.
*/
struct local
{
};

} // namespace other

/** A shape this corpus documents.

    Its outline is a @ref geom::circle, from a set Doxygen documented, and
    it reports that circle's @ref geom::circle::area; @ref geom::box,
    @ref geom::pi and @ref geom::fill live in the same set. Written with a
    leading `::`, @ref ::other::socket names the class it always did.

    With a parameter list or template arguments, a name links just the
    same: @ref geom::circle::area(), @ref geom::box<3> and
    @ref geom::box<3>::volume().
*/
struct outline
{
};

/** An adapter this corpus documents.

    It adapts an @ref ns::response_factory, from a set MrDocs documented,
    and calls its @ref ns::response_factory::make(); that set also
    documents @ref ns::outer::config::apply.
*/
struct adapter
{
};
