// Only macros matching `include-macros` are extracted. Namespace-scoped
// `include-symbols` never restricts macros, so the allowlist here is what
// scopes them by name.

/// Public API version.
#define MYLIB_VERSION 1

/// Public assert.
#define MYLIB_ASSERT(x) ((x) ? (void)0 : abort())

/// Implementation detail, still under MYLIB_, so still allowed.
#define MYLIB_DETAIL_INTERNAL 0

/// Unrelated macro outside the allowlist, dropped.
#define UNRELATED 42
