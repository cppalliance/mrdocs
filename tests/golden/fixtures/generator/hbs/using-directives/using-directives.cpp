namespace app {

/// A namespace that is part of the public API.
namespace visible { void f(); }

namespace secret { void g(); }

using namespace visible;
using namespace secret;

}
