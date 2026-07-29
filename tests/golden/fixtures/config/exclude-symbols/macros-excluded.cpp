// Macros matching the exclude pattern are dropped.

/// Public macro, kept.
#define MYLIB_VERSION 1

/// Implementation detail, dropped by the exclude pattern.
#define MYLIB_DETAIL_INTERNAL 0

/// Public assert, kept.
#define MYLIB_ASSERT(x) ((x) ? (void)0 : abort())

/// Another internal helper, dropped.
#define MYLIB_DETAIL_HELPER(y) ((y) + 1)
