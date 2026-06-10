/// A small two-dimensional point.
struct Point
{
    /// Distance from the origin.
    double length() const;

    /// Translate by an offset.
    void translate(int dx, int dy);
};
