// A parameter documented on more than one redeclaration of the same function
// must be kept once, not duplicated (and must not raise a spurious duplicate
// warning). `dst` is documented on both declarations of `copy`; the merged doc
// keeps a single `dst` entry alongside `src`.

/// Copies a buffer.
/// \param dst The destination buffer.
void copy(char* dst, char const* src);

/// \param dst The destination buffer.
/// \param src The source buffer.
void copy(char* dst, char const* src);
