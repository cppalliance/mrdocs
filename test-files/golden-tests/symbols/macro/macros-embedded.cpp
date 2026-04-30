// Single-page output in embedded mode with at least one
// macro present. Exercises the embedded-mode branch of
// `HandlebarsGenerator::buildOne`, which is otherwise
// unreached by the wrapped-mode default of the rest of the
// suite.

/// A simple object-like macro.
#define EMBED_OBJECT 1

/** A simple function-like macro.

    @param x The argument.
*/
#define EMBED_FUNC(x) (x)
