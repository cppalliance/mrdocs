#ifndef GEO_POINT_HPP
#define GEO_POINT_HPP

namespace geo {

/** A point in two-dimensional space.

    @tparam T The coordinate type (`int`, `double`, etc.).
*/
template <class T>
struct point
{
    T x;
    T y;
};

} // namespace geo

#endif
