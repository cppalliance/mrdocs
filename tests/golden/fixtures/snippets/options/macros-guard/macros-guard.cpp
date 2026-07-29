// The real definition exists only where the platform provides 128-bit
// integers, which many toolchains (MSVC, 32-bit targets) do not, so
// MrDocs may never see this #define.
#if defined(__SIZEOF_INT128__)
#define MY_LIB_HAS_INT128 1
#endif

// MrDocs always defines __MRDOCS__, so a guarded definition lets the
// feature-test macro be documented regardless of the platform.
#ifdef __MRDOCS__
/// Defined by the library when 128-bit integer support is available.
#define MY_LIB_HAS_INT128
#endif
