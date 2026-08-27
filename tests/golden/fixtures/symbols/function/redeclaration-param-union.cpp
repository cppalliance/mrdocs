// Different declarations of one function may each document a different entry in
// the same list section. Merging must union them rather than keep only the
// first declaration's: here one declaration of `copy` documents `dst` and the
// other documents `src`, so the merged doc must carry both.

/// Copies a buffer.
/// \param dst The destination buffer.
void copy(char* dst, char const* src);

/// \param src The source buffer.
void copy(char* dst, char const* src);
