struct abs_fn
{
    /** Compute the absolute value.

        @param x The input value.
        @return The absolute value of x.
    */
    double
    operator()(double x) const noexcept;
};

/** Return the absolute value of a number. */
constexpr abs_fn abs = {};
