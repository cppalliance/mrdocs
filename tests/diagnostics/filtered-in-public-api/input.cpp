namespace detail {

/** A helper the filters keep out of the documentation.
*/
struct my_shame {};

/** A template the filters keep out of the documentation.

    @tparam U The type the template is about.
*/
template<class U>
struct impl {};

} // namespace detail

/** A documented template, to give a filtered type as an argument.

    @tparam U The type held.
*/
template<class U>
struct box {};

/** A public class deriving from a filtered base.
*/
struct T : detail::my_shame {};

/** A public function returning a filtered type.

    @return Something the reader cannot follow.
*/
detail::my_shame publicFunction();

/** A public function taking a filtered type.

    @param shame Something the reader cannot follow.
*/
void takeShame(detail::my_shame const& shame);

/** An alias of a filtered type.
*/
using shame_alias = detail::my_shame;

/** A variable of a filtered type.
*/
extern detail::my_shame shameVariable;

/** A class with a data member of a filtered type.
*/
struct holder
{
    /** A data member of a filtered type.
    */
    detail::my_shame member;
};

/** A public function returning a documented template of a filtered type.

    @return Something the reader cannot follow.
*/
box<detail::my_shame> boxedShame();

/** A public function returning a specialization of a filtered template.

    @return Something the reader cannot follow.
*/
detail::impl<int> makeImpl();
