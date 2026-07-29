/// A base shape with one operation.
struct shape
{
    /// Render the shape to the screen.
    void draw();
};

/// A circle. The `draw` from `shape` is inherited
/// and listed on the circle's page too.
struct circle : shape
{
    /// Construct a circle of the given radius.
    explicit circle(double r);
};
