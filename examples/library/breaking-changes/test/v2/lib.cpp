/// A point on the plane.
struct Point
{
    /// X coordinate.
    int x;

    /// Y coordinate.
    int y;

    /** Distance from the origin.

        @return The Euclidean length of the vector.
    */
    double length() const noexcept;
};

/// Compute the area of a circle.
double area_of_circle(double radius);

/// New helper added in v2.
double area_of_square(double side);
