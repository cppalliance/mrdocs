/// A small library of geometric helpers.
namespace geometry {

/// The supported coordinate systems.
enum class System
{
    /// Distances along two perpendicular axes.
    cartesian,
    /// A distance and an angle measure.
    polar
};

/// A point in two dimensions.
struct Point
{
    /** Compute the distance from the origin.

        @return The distance from the origin.
    */
    double length() const;

    /** Translate by an offset.

        @param dx The offset along the first axis.
        @param dy The offset along the second axis.
    */
    void translate(double dx, double dy);
};

/// A distance in the units of the coordinate system.
using Distance = double;

/// The coordinate system the helpers assume.
extern System defaultSystem;

} // namespace geometry
