/// Application value types.
namespace app {

/// A point or displacement in the plane.
struct Vec2
{
    /// The x component.
    double x;
    /// The y component.
    double y;

    /// The Euclidean length.
    ///
    /// @return The straight-line length of the vector.
    double length() const;
};

/// A circle in the plane.
struct Circle
{
    /// The radius.
    double radius;

    /// The enclosed area.
    ///
    /// @return The area enclosed by the circle.
    double area() const;
};

/// The distance between two points.
///
/// @param a The first point.
/// @param b The second point.
/// @return The straight-line distance between `a` and `b`.
double distance(Vec2 const& a, Vec2 const& b);

}
