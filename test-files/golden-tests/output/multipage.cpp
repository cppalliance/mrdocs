// NOTE: This golden test uses multipage output to validate cross-page links produced for
// nested namespaces/types. Single-page output cannot exercise the relative paths.

namespace alpha {

namespace beta {

/// Widget with its own page
struct Widget {};

/// Factory living under a nested namespace
Widget make_widget();

} // namespace beta

/// Uses a nested type to force cross-page links and asset resolution.
beta::Widget use_widget(beta::Widget);

} // namespace alpha
