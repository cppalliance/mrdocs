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
    double length() const;
};

/// Compute the area of a circle.
double area_of_circle(double radius);

/// Helper that should be removed in v2.
int legacy_helper();
