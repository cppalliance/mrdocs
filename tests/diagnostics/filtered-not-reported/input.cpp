namespace detail {

/** A tag the caller can hold but not name.

    @implementationdefined
*/
struct token {};

/** A helper the filters keep out of the documentation.
*/
struct hidden {};

} // namespace detail

/** A documented template.

    @tparam U The type held.
*/
template<class U>
struct box {};

/** A documented template with a member.

    @tparam U The type the template is about.
*/
template<class U>
struct outer
{
    /** A member of the template.
    */
    struct inner {};
};

/** A public class deriving privately from a filtered base.
*/
struct derived : private detail::hidden {};

/** A public function returning a filtered type marked as such.

    @return A token that releases the widget when destroyed.
*/
detail::token acquire();

/** A public function returning a specialization of a documented template.

    @return A box of `int`.
*/
box<int> makeBox();

/** A public function returning a member of a specialization.

    @return A member the primary template documents.
*/
outer<int>::inner makeInner();
