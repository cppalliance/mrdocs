// A macro and regular symbols coexist in a single corpus. The macro
// lands in the global namespace's macro list, while the namespace,
// function, and struct keep their usual places. Their SymbolIDs never
// collide: a macro ID hashes "macro:NAME@path:line" whereas a
// declaration ID hashes the Clang USR, so the two id spaces are
// disjoint by construction.

/// A documented function-like macro.
#define WIDGET_ASSERT(cond) ((void)0)

namespace widget {

/// A regular function.
void configure();

/// A regular struct.
struct Config {};

} // namespace widget
