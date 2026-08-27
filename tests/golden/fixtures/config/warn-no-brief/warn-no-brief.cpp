// `warn-no-brief` flags a symbol that is documented but has no brief. Auto-brief
// turns a leading description into the brief, so a symbol documented only with a
// block command such as `@return` has documentation yet no brief, which is
// exactly what this option reports. The warning goes to stderr, so this fixture
// pins the corpus: `compute` carries its `@return` but no brief, while `scale`,
// which has a leading description, keeps its auto-brief.

/// \return The computed value.
int compute();

/// Scales a value by a factor.
/// \return The scaled value.
int scale(int factor);
