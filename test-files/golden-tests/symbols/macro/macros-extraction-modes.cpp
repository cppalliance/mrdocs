// Exercises the `implementation-defined` and `see-below`
// filters. The existing `macros-excluded.cpp` and
// `macros-allowlist.cpp` cover only exclude / include.

/// A regular macro (not matched by any filter).
#define MYLIB_REGULAR 1

/// Macro matched by `implementation-defined`. The synopsis
/// should be elided in the rendered output.
#define MYLIB_IMPL_DETAIL 0

/** A real-world example of macro using "see below".

    Assert that @p cond holds; otherwise terminate with a
    diagnostic that includes @p msg.

    Evaluates @p cond. If the result is non-zero, the macro
    has no observable effect. Otherwise, it reports a
    diagnostic that includes @p msg and the source location
    of the failing call, then terminates the program.

    The diagnostic format and the termination mechanism are
    implementation details that may change between releases.
    Clients should rely only on the behavior described here.

    @param cond Any expression contextually convertible
        to `bool`.
    @param msg A null-terminated C string describing
        the assertion.
*/
#define MYLIB_SEE_BELOW(cond, msg)                       \
    ((cond) ? (void)0                                    \
            : ::mylib_detail::assert_fail(               \
                  msg, __FILE__, __LINE__))
