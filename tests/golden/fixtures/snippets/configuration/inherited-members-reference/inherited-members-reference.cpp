/// A drawable figure.
struct figure
{
    /// Render the figure.
    void draw();
};

/// A square.
struct square : figure
{
    /// Construct a square of the given side.
    explicit square(double side);
};
