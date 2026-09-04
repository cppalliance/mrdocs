// `max-errors` caps how many documentation errors MrDocs reports, like a
// compiler's -fmax-errors, and only takes effect together with warn-as-error.
// The diagnostics go to stderr, so this fixture only pins that the option is
// accepted and does not change the corpus: a single translation unit is never
// cut short, so every symbol is still extracted even with more undocumented
// symbols than the cap.

/// A documented function.
void documented();

void undocumented_a();

void undocumented_b();

void undocumented_c();
