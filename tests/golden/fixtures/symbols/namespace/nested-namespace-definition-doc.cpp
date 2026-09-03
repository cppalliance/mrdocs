// A comment on a nested-namespace-definition documents the innermost
// namespace it opens. The enclosing components are only the path to it and
// keep whatever documentation their own declarations carry.

/// The outer namespace, documented where it is opened on its own.
namespace outer {
}

/// The inner namespace.
namespace outer::inner {
/// A function.
void f();
}

namespace other {
}

/// Deepest of three.
namespace other::middle::deepest {
/// A function.
void g();
}
