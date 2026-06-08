struct scale_fn
{
    /** Returns `x` scaled by the configured factor. */
    int operator()(int x) const;

    int factor; // extra member: auto-detection skips this type
};

/** @functionobject

    The extra `factor` member defeats auto-detection.
 */
constexpr scale_fn scale = {2};
