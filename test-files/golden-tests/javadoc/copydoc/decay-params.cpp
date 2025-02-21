/** We should not copy this doc

    `bar` should not copy this documentation
    just because it has the same name and is
    simpler.
 */
void
foo();

/** We should not copy this doc

    `bar` should not copy this documentation
    just because it has the same number of
    parameters.

    @param a An integer.
 */
void
foo(int a);

/** Brief from `foo()`

    This is the function original documentation
    for the foo function with an array parameter.

    @param a An array of integers.
    @return A boolean value.
*/
constexpr
bool
foo(int a[3]);

/** @copydoc foo(int*)
 */
constexpr
bool
bar(int *a);
