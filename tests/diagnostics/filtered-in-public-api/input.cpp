namespace detail {

/** A helper the filters keep out of the documentation.
*/
struct my_shame {};

} // namespace detail

/** A public class deriving from a filtered base.
*/
struct T : detail::my_shame {};

/** A public function returning a filtered type.

    @return Something the reader cannot follow.
*/
detail::my_shame publicFunction();
