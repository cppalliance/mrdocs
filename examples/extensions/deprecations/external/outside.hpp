/// A header outside the source root of the sample project, so that the DOM
/// has no source path for what it declares.
namespace outside {

/// A function declared outside the source root.
[[deprecated("no path to show")]]
void gone();

} // namespace outside
