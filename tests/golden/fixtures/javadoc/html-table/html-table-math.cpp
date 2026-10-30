/** Compute the norm of a vector.

    <table>
      <tr><th>Norm</th><th>Formula</th></tr>
      <tr><td>Euclidean</td><td>@f$ \sqrt{x^2 + y^2} @f$</td></tr>
      <tr><td>Maximum</td><td>$\max(|x|, |y|)$</td></tr>
    </table>

    \f$p\f$ selects the norm.

    The result is \f$\|v\|_p\f$.

    \f$p = 2\f$ is the default.
 */
double norm(double x, double y, int p);
