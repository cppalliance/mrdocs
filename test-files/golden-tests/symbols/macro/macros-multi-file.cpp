// Multi-file macro extraction. The companion header
// `macros-multi-file.hpp` defines `SHARED_MACRO`; this
// translation unit `#undef`s it and redefines it. The
// resulting corpus has two macro symbols sharing the
// `SHARED_MACRO` name but living in different files.

#include "macros-multi-file.hpp"

#undef SHARED_MACRO
#define SHARED_MACRO 2
