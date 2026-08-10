#ifndef APP_TEXT_HPP
#define APP_TEXT_HPP

namespace app {

/** The number of characters before the terminating null.

    Counts the bytes up to, but not including, the null terminator.

    @par Example

    @code
    assert(app::length("abc") == 3);
    @endcode
*/
int length(char const* s);

/** True when the string has no characters.

    Only the empty string is blank; any other string, including whitespace, is
    not.

    @par Examples

    @code
    assert(app::is_empty(""));
    @endcode

    @code
    assert(!app::is_empty("abc"));
    @endcode
*/
bool is_empty(char const* s);

} // namespace app

#endif
