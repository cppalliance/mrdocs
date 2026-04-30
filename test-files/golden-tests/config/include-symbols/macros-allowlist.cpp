// Only macros matching the allowlist pattern are extracted.

/// Public API version.
#define MYLIB_VERSION 1

/// Public assert.
#define MYLIB_ASSERT(x) ((x) ? (void)0 : abort())

/// Implementation detail (still under MYLIB_, so still allowed).
#define MYLIB_DETAIL_INTERNAL 0

/// Unrelated macro outside the allowlist.
#define UNRELATED 42
