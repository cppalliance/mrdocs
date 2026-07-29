/// A point in 2D.
struct point
{
    double x;
    double y;

    /// Equality comparison, declared as a friend so it
    /// participates in ADL.
    friend bool operator==(point a, point b);
};
