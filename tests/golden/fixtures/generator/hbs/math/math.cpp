/** Solves a quadratic equation in $O(1)$ time.

    The roots are \f$x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}\f$.

    A standalone Markdown display span is promoted to a block:

    $$\int_0^1 x^2\,dx = \frac{1}{3}$$

    The Doxygen delimiters produce the same block form:

    \f[
    e^{i\pi} + 1 = 0
    \f]
 */
double solve_quadratic(double a, double b, double c);
