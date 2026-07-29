// Macros matching `exclude-macros` are dropped even when documented.

/// Public macro, kept.
#define MYLIB_VERSION 1

/// Implementation detail, dropped by the exclude pattern.
#define MYLIB_DETAIL_INTERNAL 0

/// Public assert, kept.
#define MYLIB_ASSERT(x) ((x) ? (void)0 : abort())
