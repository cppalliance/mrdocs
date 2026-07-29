/** Computes the area of a circle: $A = \pi r^2$.

    The Gaussian integral is the classic closed-form result:

    $$\int_{-\infty}^{\infty} e^{-x^2}\,dx = \sqrt{\pi}$$
 */
double circle_area(double r);

/** Looks up a key in a sorted range in \f$O(\log n)\f$ time.

    The number of comparisons follows the recurrence:

    \f[ T(n) = T(n/2) + O(1) \f]

    This comment uses the Doxygen formula delimiters, which Clang parses as
    known commands, so it compiles cleanly under -Wdocumentation.
 */
int binary_search(int const* first, int n, int key);
