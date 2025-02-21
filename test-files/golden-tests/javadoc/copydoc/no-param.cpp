/** Brief from `foo()`

    This is the function original documentation
    for the foo function with no parameters.

    @return A boolean value.
*/
constexpr
bool
foo();

/** @copydoc foo()

    @param ch A character.
 */
constexpr
bool
foo(char ch);

/** @copydoc foo()

    This documentation is copied from the
    page containing the foo overload with
    no parameters.
 */
constexpr
bool
copyFromNoParam();

/** @copydoc foo

    This documentation is copied from the
    page containing all overloads of foo.

 */
constexpr
bool
copyfromOverloads();