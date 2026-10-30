namespace detail {

/// Brief of helper.
void helper();

} // detail

namespace shown {

/// Brief of other.
void other();

} // shown

/// Brief of outer.
namespace outer {

/// \cond INTERNAL
using namespace detail;
/// \endcond

using namespace shown;

/// Brief of member.
void member();

} // outer
