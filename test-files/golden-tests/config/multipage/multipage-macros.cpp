// Multipage rendering with corpus-level macros.
//
// Verifies that:
//   - A `macros.{ext}` index page is generated.
//   - The global-namespace page carries a "See also: Macros"
//     navigation hint.
//   - Each macro still gets its own per-symbol page.

namespace foo {

/// A function in a namespace.
void bar();

} // namespace foo

/// Object-like macro.
#define MY_VERSION 1

/** Function-like macro.

    @param x The argument.
*/
#define MY_INC(x) ((x) + 1)
