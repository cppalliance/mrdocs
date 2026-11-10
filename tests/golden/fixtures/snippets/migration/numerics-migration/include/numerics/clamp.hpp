namespace numerics {
#if defined(NUMERICS_DOCUMENTATION_BUILD)
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
T clamp(T const& v, T const& lo, T const& hi);
#else
struct clamp_fn
{
    template <class T>
    T operator()(T const& v, T const& lo, T const& hi) const;
};
inline constexpr clamp_fn clamp = {};
#endif
} // namespace numerics
