namespace detail {
/** The compiled bytecode the matcher executes.

    @implementationdefined
 */
struct program;
}

/** A compiled regular expression.

    Holds the program built from a pattern and tests it against input
    text. Create one with `compile` rather than constructing it directly.

    @seebelow
 */
class regex
{
    detail::program* prog_;
public:
    bool match(char const* text) const;
};

struct compile_fn
{
    /** Compiles `pattern` into a regular expression. */
    regex operator()(char const* pattern) const;

    int syntax; // extra member: auto-detection skips this type
};

/** @functionobject

    The extra `syntax` member defeats auto-detection.
 */
constexpr compile_fn compile = {};

/** Exposes a regex's compiled bytecode, for debugging.

    @returns The bytecode, in an implementation-defined form.
 */
detail::program const& bytecode(regex const& re);
