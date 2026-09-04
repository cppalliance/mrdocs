A `Vec2` stores a pair of Cartesian coordinates and doubles as both a point
and a displacement. Arithmetic on it follows the usual rules for a 2-D vector
space, so adding two vectors adds their components and scaling multiplies them.

Prefer `Vec2` over a bare `std::pair<double, double>` when the values are
geometric: the named `x` and `y` members and the geometry helpers make intent
obvious at the call site, and keep unrelated pairs from being mixed in by
mistake.
