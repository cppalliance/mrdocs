/** Computes the area of a circle: $A = \pi r^2$.

    For very large radii the result is taken modulo
    floating-point precision: $$A_{approx} = \pi r^2 + \epsilon$$
 */
double circle_area(double r);
