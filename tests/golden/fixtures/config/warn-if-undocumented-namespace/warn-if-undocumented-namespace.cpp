/// A documented function so the regular undocumented check stays quiet.
void documented();

namespace named_namespace {
/// A documented symbol inside an otherwise undocumented named namespace.
///
/// With warn-if-undocumented-namespace enabled, `named_namespace` itself is
/// reported as undocumented (the warning goes to stderr, which the golden
/// harness does not capture yet).
void inside();
}

/// The inline namespace is transparent: its members are reparented and it is
/// never required to be documented, regardless of the option.
inline namespace transparent {
/// A documented symbol reached through the transparent inline namespace.
void deep();
}
