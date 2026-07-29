// With `extract-all-macros` on, undocumented macros are extracted too,
// not just the documented ones that show by default.

#define UNDOC_OBJECT 1

/// Documented object-like macro.
#define DOC_OBJECT 2

#define UNDOC_FUNC(x) (x)

/// Documented function-like macro.
#define DOC_FUNC(x) (x)
