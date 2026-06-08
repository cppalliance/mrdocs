/** A two-dimensional point.
*/
struct point
{
    double x;
    double y;
};

/** Print a point to stdout.

    The free function takes `point` as its only
    parameter, so it gets attached to `point` as a
    related function.
*/
void print(point p);
