// Object-like macros with various body shapes.
/// A documentation comment.
#define FOO 1
#define PI 3.14
#define GREETING "hello"

// Object-like macro that expands to nothing.
#define EMPTY

/** Add two values.

    @param a First operand.
    @param b Second operand.
*/
#define ADD(a, b) ((a) + (b))

/** Log @p msg to `stderr`, tagged with the current file and line number.

    @param msg The message to log.
*/
#define LOG(msg) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, msg)

// Variadic macro.
#define LOG_F(fmt, ...) printf(fmt, __VA_ARGS__)

// Variadic-only (no named parameters).
#define VLOG(...) printf(__VA_ARGS__)

// Body spread across lines via backslash continuation.
#define ANSWER  \
    (40         \
    +           \
    2)

// Function-like macro with line continuation.
#define MIN(a, b)        \
    ((a) < (b)           \
        ? (a)            \
        : (b))

// Macro definition with whitespace between `#` and `define`, plus
// backslashes. The backslashes should remain aligned in the output,
// despite the removal of the whitespace between `#` and `define`.
#   define SQUARE(x) (  \
    (x)                 \
    *                   \
    (x))

// Body with a continuation line having fewer leading
// whitespace chars than were stripped from the `#define`
// line.
#   define MISALIGNED(x) ( \
  (x) )

/** Sum a variadic list of values.

    @param ... The values to sum.
*/
#define SUM(...) (__VA_ARGS__)

/** Variadic `printf` wrapper, documenting the variadic
    argument list with the conventional `...` form.

    @param fmt The format string.
    @param ... Format arguments.
*/
#define PRINTF(fmt, ...) printf(fmt, __VA_ARGS__)

/** Variadic printf wrapper, documented with the explicit
    `__VA_ARGS__` name.

    @param fmt The format string.
    @param __VA_ARGS__ Format arguments.
*/
#define VPRINTF(fmt, ...) printf(fmt, __VA_ARGS__)

/** Compute the median of three values.

    @param a,b,c The three values to compare.
*/
#define MEDIAN3(a, b, c) \
    (((a) > (b)) ? \
        (((b) > (c)) ? (b) : (((a) > (c)) ? (c) : (a))) : \
        (((a) > (c)) ? (a) : (((b) > (c)) ? (c) : (b))))

// --- Intentionally bad doc comments. These exercise the
// parameter-doc validation warnings that fire during
// finalization. The macros still render normally; the
// warnings go to `stderr` only. ---

/** Duplicate parameter documentation.

    @param x One thing.
    @param x Same thing again.
*/
#define DUP(x) (x)

/** Documentation that names a parameter the macro
    does not actually have.

    @param y Not actually a parameter of WRONG_PARAM.
*/
#define WRONG_PARAM(x) (x)

/** Non-variadic macro that mistakenly documents a
    variadic argument list.

    @param x The argument.
    @param ... Not really variadic.
*/
#define NON_VARIADIC(x) (x)

/** Object-like macro with a spurious parameter
    documentation block.

    @param x Should not be here.
*/
#define OBJECT_WITH_PARAM 1
