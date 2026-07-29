/// A drawable shape.
struct shape
{
    /// Render the shape.
    void draw();
};

/// A circle.
struct circle : shape
{
    /// Construct a circle of the given radius.
    explicit circle(double r);
};
