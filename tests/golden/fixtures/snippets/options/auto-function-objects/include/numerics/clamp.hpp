#ifndef NUMERICS_CLAMP_HPP
#define NUMERICS_CLAMP_HPP

// tag::body[]
namespace numerics {

struct clamp_fn
{
    /** Constrain a value to the inclusive range `[lo, hi]`.

        Returns `lo` when `v < lo`, `hi` when `hi < v`, otherwise `v`.
        Behavior is undefined if `hi < lo`.

        @tparam T  A type comparable with `operator<`.
        @param  v  The value to constrain.
        @param  lo The inclusive lower bound.
        @param  hi The inclusive upper bound.
        @return A copy of `v`, `lo`, or `hi`, whichever lies within the range.
    */
    template <class T>
    T operator()(T const& v, T const& lo, T const& hi) const;
};

inline constexpr clamp_fn clamp = {};

}
// end::body[]

#endif
