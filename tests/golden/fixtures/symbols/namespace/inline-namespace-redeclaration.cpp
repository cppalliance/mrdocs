// An inline namespace is transparent: MrDocs skips it in the parent chain, so a
// symbol reached through the inline namespace and the same symbol named past it
// are one entity and must share a SymbolID. This holds whether the inline
// namespace sits directly above the symbol or higher up the path.

namespace app {

inline namespace abi {

/// Perform the action.
///
/// Defined directly inside the inline ABI namespace.
void widget();

namespace inner {
/// A helper reached through the inline namespace: app::abi::inner::helper.
void helper();
}

} // namespace abi

/// A redeclaration naming the function directly in the enclosing namespace,
/// past the transparent inline namespace.
void widget();

namespace inner {
/// The same helper named past the inline namespace: app::inner::helper.
void helper();
}

} // namespace app
