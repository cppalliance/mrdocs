// Intermediary helper used to build the public macro below. It has an
// ordinary comment, not a documentation comment, so it stays private.
#define MY_LIB_DETAIL_STRINGIZE(x) #x

/// The library version, as a string literal.
#define MY_LIB_VERSION_STRING MY_LIB_DETAIL_STRINGIZE(1.0)
