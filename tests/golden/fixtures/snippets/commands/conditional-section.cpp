/** Parse a document.

    @cond INTERNAL
    The parser reuses an internal buffer between calls.
    @endcond

    Return the number of values read.
 */
int parse(char const* text);

/// @cond INTERNAL
namespace detail {

/// Clear the internal buffer.
void reset();

} // namespace detail
/// @endcond
