#include <geo/point.hpp>
#include <cmath>

namespace geo {

double distance(point a, point b)
{
    double const dx = a.x - b.x;
    double const dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace geo
