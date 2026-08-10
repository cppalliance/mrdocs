#ifndef APP_CIRCLE_HPP
#define APP_CIRCLE_HPP

#include "shape.hpp"

namespace app {

/// A circle.
struct Circle : Shape
{
    /// The radius.
    double radius;

    /// The enclosed area.
    double area() const;
};

} // namespace app

#endif
