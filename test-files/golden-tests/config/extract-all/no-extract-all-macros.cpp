// Undocumented macros are dropped when `extract-all` is off.

#define UNDOC_OBJECT 1

/// Documented object-like macro.
#define DOC_OBJECT 2

#define UNDOC_FUNC(x) (x)

/** Documented function-like macro.

    @param x Argument.
*/
#define DOC_FUNC(x) (x)
