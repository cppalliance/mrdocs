#ifndef GEO_AREA_HPP
#define GEO_AREA_HPP

/// Area calculations for simple shapes.
namespace geo {

/** The area of a rectangle.

    @return The area, `width * height`.
*/
double rectangle_area(double width, double height);

/// The area of a circle.
double circle_area(double radius);

}

#endif
