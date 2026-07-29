// Rendering coverage for documented macros: the single-page
// `Macros` section heading, the `signature/macro.hbs` synopsis
// (object-like, function-like, variadic) and the doc comment.

/// The library's ABI version.
#define EXAMPLE_ABI_VERSION 3

/** Clamp `x` to the closed range [`lo`, `hi`].

    @param x The value to clamp.
    @param lo The lower bound.
    @param hi The upper bound.
    @return `x` clamped to the range.
*/
#define EXAMPLE_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/** Log a formatted message.

    @param fmt A `printf`-style format string.
    @param ... Arguments referenced by `fmt`.
*/
#define EXAMPLE_LOG(fmt, ...) ::example::log_impl((fmt), __VA_ARGS__)
