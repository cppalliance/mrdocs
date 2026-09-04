#ifndef APP_VEC2_HPP
#define APP_VEC2_HPP

namespace app {

/// A 2-D vector.
struct Vec2
{
    /// The x component.
    double x;
    /// The y component.
    double y;

    /// The Euclidean length.
    double length() const;
};

} // namespace app

#endif
