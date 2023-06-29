extern "C" {

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcast-qual"
# pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-qual"
# pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4334) // result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#endif

#include "../lua/src/lapi.c"
#include "../lua/src/lauxlib.c"
#include "../lua/src/lbaselib.c"
#include "../lua/src/lcode.c"
#include "../lua/src/lcorolib.c"
#include "../lua/src/lctype.c"
#include "../lua/src/ldblib.c"
#include "../lua/src/ldebug.c"
#include "../lua/src/ldo.c"
#include "../lua/src/ldump.c"
#include "../lua/src/lfunc.c"
#include "../lua/src/lgc.c"
#include "../lua/src/linit.c"
#include "../lua/src/liolib.c"
#include "../lua/src/llex.c"
#include "../lua/src/lmathlib.c"
#include "../lua/src/lmem.c"
#include "../lua/src/loadlib.c"
#include "../lua/src/lobject.c"
#include "../lua/src/lopcodes.c"
#include "../lua/src/loslib.c"
#include "../lua/src/lparser.c"
#include "../lua/src/lstate.c"
#include "../lua/src/lstring.c"
#include "../lua/src/lstrlib.c"
#include "../lua/src/ltable.c"
#include "../lua/src/ltablib.c"
#include "../lua/src/ltm.c"
//#include "../lua/src/lua.c"
//#include "../lua/src/luac.c"
#include "../lua/src/lundump.c"
#include "../lua/src/lutf8lib.c"
#include "../lua/src/lvm.c"
#include "../lua/src/lzio.c"

#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#elif defined(_MSC_VER)
# pragma warning(pop)
#endif

} // "C"
