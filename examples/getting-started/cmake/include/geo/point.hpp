#ifndef GEO_POINT_HPP
#define GEO_POINT_HPP

namespace geo {

/** A point in two-dimensional space.
*/
struct point
{
    /// X coordinate.
    double x;
    /// Y coordinate.
    double y;
};

/** Compute the Euclidean distance between two points.

    @param a The first point.
    @param b The second point.
    @return The Euclidean distance between `a` and `b`.
*/
double distance(point a, point b);

} // namespace geo

#endif
