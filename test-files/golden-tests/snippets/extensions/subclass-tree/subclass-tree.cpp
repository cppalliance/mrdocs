/// The root of the shape hierarchy.
struct Shape {};

/// A shape with straight sides.
struct Polygon : Shape {};

/// A three-sided polygon.
struct Triangle : Polygon {};

/// A four-sided polygon.
struct Quadrilateral : Polygon {};

/// A quadrilateral with equal sides.
struct Square : Quadrilateral {};

/// A round shape.
struct Circle : Shape {};
