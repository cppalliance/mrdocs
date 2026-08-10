#ifndef APP_ARITHMETIC_HPP
#define APP_ARITHMETIC_HPP

namespace app {

/** Add two integers.

    Returns the arithmetic sum of the two arguments. The example lives in this
    comment and is exported to a file the build compiles, so it never falls out
    of date.

    @par Example

    @code
    int s = app::add(2, 3);
    assert(s == 5);
    @endcode
*/
int add(int a, int b);

/** Multiply two integers.

    Returns the product of the two arguments. Its examples are maintained in a
    separate file.
*/
int multiply(int a, int b);

/** Subtract the second integer from the first.

    Returns `a - b`. Its example is maintained in a separate file.
*/
int subtract(int a, int b);

} // namespace app

#endif
