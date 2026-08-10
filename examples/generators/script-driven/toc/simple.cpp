/// Two-dimensional geometry primitives.
namespace geo {

/// A point in the plane.
struct Point
{
    /// The x coordinate.
    double x;

    /// The y coordinate.
    double y;

    /** The distance to another point.

        @param other The point to measure to.
        @return The Euclidean distance between the two points.
    */
    double distance_to(Point other) const;
};

/// How two shapes relate spatially.
enum class Relation
{
    disjoint,    ///< No shared points.
    touching,    ///< A shared boundary only.
    overlapping  ///< Shared interior points.
};

/** The midpoint of two points.

    @param a The first point.
    @param b The second point.
    @return The point halfway between `a` and `b`.
*/
Point midpoint(Point a, Point b);

/// Coordinate-system helpers.
namespace coord {

/** Convert polar coordinates to a Cartesian point.

    @param r The radius.
    @param theta The angle in radians.
    @return The equivalent Cartesian point.
*/
Point from_polar(double r, double theta);

}

}
