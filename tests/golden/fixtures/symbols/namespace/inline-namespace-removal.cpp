// An inline namespace is transparent: MrDocs drops it from the corpus and
// reparents its members to the enclosing namespace. This holds even when the
// inline namespace is documented (e.g. an ABI-versioning namespace), so it must
// never appear as a namespace of its own in the output.

namespace lib {

/// An ABI-versioning inline namespace. Despite carrying documentation, it must
/// not surface as its own namespace; its members belong to `lib`.
inline namespace v2 {

/// A function that belongs to lib, not lib::v2.
void f();

/// A type that belongs to lib, not lib::v2.
struct Widget {};

} // namespace v2

} // namespace lib
