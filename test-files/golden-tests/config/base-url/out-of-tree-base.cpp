// When `base-url` is set, in-tree symbols carry a source link in their
// Synopsis. A symbol inherited from a base declared outside `source-root`
// (here, an out-of-tree header) has no path relative to the root, so it must
// not receive a source link. The location is rendered as plain text instead
// of a broken URL.

#include <external_base.hpp>

namespace test {

/// A class declared in-tree that inherits an out-of-tree base.
struct InTree : ExternalBase
{
    /// A member function declared in-tree.
    void inTreeMethod();
};

} // namespace test
