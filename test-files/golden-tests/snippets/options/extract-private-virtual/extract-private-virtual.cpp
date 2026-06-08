/// A base class declaring a virtual `draw` method.
struct shape
{
    virtual ~shape() = default;
    /// Render the shape.
    virtual void draw() = 0;
};

/// A circle implementation.
class circle : public shape
{
public:
    /// Construct a circle of the given radius.
    explicit circle(double r);

private:
    /// The private override of `shape::draw`.
    void draw() override;

    double radius;
};
