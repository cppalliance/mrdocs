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

#include "../third-party/lua/src/lapi.c"
#include "../third-party/lua/src/lauxlib.c"
#include "../third-party/lua/src/lbaselib.c"
#include "../third-party/lua/src/lcode.c"
#include "../third-party/lua/src/lcorolib.c"
#include "../third-party/lua/src/lctype.c"
#include "../third-party/lua/src/ldblib.c"
#include "../third-party/lua/src/ldebug.c"
#include "../third-party/lua/src/ldo.c"
#include "../third-party/lua/src/ldump.c"
#include "../third-party/lua/src/lfunc.c"
#include "../third-party/lua/src/lgc.c"
#include "../third-party/lua/src/linit.c"
#include "../third-party/lua/src/liolib.c"
#include "../third-party/lua/src/llex.c"
#include "../third-party/lua/src/lmathlib.c"
#include "../third-party/lua/src/lmem.c"
#include "../third-party/lua/src/loadlib.c"
#include "../third-party/lua/src/lobject.c"
#include "../third-party/lua/src/lopcodes.c"
#include "../third-party/lua/src/loslib.c"
#include "../third-party/lua/src/lparser.c"
#include "../third-party/lua/src/lstate.c"
#include "../third-party/lua/src/lstring.c"
#include "../third-party/lua/src/lstrlib.c"
#include "../third-party/lua/src/ltable.c"
#include "../third-party/lua/src/ltablib.c"
#include "../third-party/lua/src/ltm.c"
//#include "../third-party/lua/src/lua.c"
//#include "../third-party/lua/src/luac.c"
#include "../third-party/lua/src/lundump.c"
#include "../third-party/lua/src/lutf8lib.c"
#include "../third-party/lua/src/lvm.c"
#include "../third-party/lua/src/lzio.c"

#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#elif defined(_MSC_VER)
# pragma warning(pop)
#endif

} // "C"
