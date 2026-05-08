// A trivial input that exercises the addon-defined mock-md generator.
// The function name contains an underscore so the single-byte escape
// rule '_' -> '\_' fires on it; the doc-comment's brief begins with
// the literal token TODO so the multi-byte rule 'TODO' -> '[!]' fires
// there during rendering.

/// TODO write me
void my_function();
