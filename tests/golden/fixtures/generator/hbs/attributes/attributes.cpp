// A generator rendering example: how C++ attributes appear in the
// Handlebars-generated pages (adoc and HTML).
//
// Unlike the XML corpus tests under symbols/, this exercises the
// templates: the `[[...]]` clause in each kind's signature, the
// attribute admonitions, and the `[deprecated]`/`[noreturn]` tags in
// member tables.
// It is deliberately small because the HTML/adoc generators are
// expensive; it just needs to cover the renderable cases once.

/// A deprecated class.
struct [[deprecated("use Widget2")]] Widget {};

/// A class deprecated via the GNU spelling, normalized to the standard form.
struct [[gnu::deprecated("use Gnu2")]] Gnu {};

/// An over-aligned type (a single, non-string attribute argument).
struct [[gnu::aligned(16)]] Aligned {};

/// A deprecated scoped enum with a deprecated enumerator.
enum class [[deprecated("use Color2")]] Color
{
    Red,
    Green [[deprecated("use Blue")]],
    Blue
};

/// A deprecated type alias (the motivating case from issue #1233).
using extension_type [[deprecated("use extent_type")]] = int;

/// A nodiscard function (an attribute with no arguments).
[[nodiscard]] int checked();

/// A deprecated function with a message.
[[deprecated("use compute2")]] int compute();

/// A printf-like function (an attribute with multiple arguments).
[[gnu::format(printf, 1, 2)]] void log_message(char const* fmt, ...);

/// A function carrying the same attribute twice with different arguments.
[[clang::annotate("a"), clang::annotate("b")]] void annotated();

/// A deprecated variable.
[[deprecated("use value2")]] inline int value = 0;

/// An overload set whose every overload is deprecated: the set is tagged.
[[deprecated("use scale2")]] void scale();
/// @copydoc scale()
[[deprecated("use scale2")]] void scale(int);

/// An overload set with only one deprecated overload: the set is not tagged.
void resize();
/// @copydoc resize()
[[deprecated("use resize2")]] void resize(int);

/// A class whose members carry attributes, to exercise member-table tags.
struct Gadget
{
    /// A deprecated member function.
    [[deprecated("use run2")]] void run();

    /// A member function that never returns.
    [[noreturn]] void fail();

    /// A deprecated data member.
    [[deprecated]] int field;
};
