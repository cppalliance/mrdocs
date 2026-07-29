/// The public assertion macro.
#define MY_LIB_ASSERT(x) ((x) ? (void)0 : ::my_lib::fail())

/// An internal helper, dropped by the exclude pattern.
#define MY_LIB_DETAIL_CHECK(x) ((void)(x))

/// The implementation behind MY_LIB_ASSERT, dropped by the exclude pattern.
#define MY_LIB_ASSERT_IMPL(x) ((void)(x))
