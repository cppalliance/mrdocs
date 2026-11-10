//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_PLATFORM_HPP
#define MRDOCS_API_PLATFORM_HPP

#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Export.hpp>
#include <type_traits>
// The standard library defines the macros the tag below reads, and <version>
// is the smallest header that defines them.
#include <version>

#if __cplusplus < 202002L
    #error "mrdocs requires at least C++20"
#endif

/*
    Platform-specific things, and stuff
    that is dependent on the toolchain.
*/

namespace mrdocs {

//------------------------------------------------

#if ! defined(__x86_64__) && ! defined(_WIN64) && ! defined(__aarch64__)
# error mrdocs requires a 64-bit architecture
#endif

#ifndef MRDOCS_NO_UNIQUE_ADDRESS
# if __has_cpp_attribute(no_unique_address)
#  define MRDOCS_NO_UNIQUE_ADDRESS [[no_unique_address]]
# elif __has_cpp_attribute(msvc::no_unique_address)
#  define MRDOCS_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
# else
#  define MRDOCS_NO_UNIQUE_ADDRESS
# endif
#endif

// Expected root of the MrDocs installation when package is found
#ifndef MRDOCS_DEFAULT_ROOT
#define MRDOCS_DEFAULT_ROOT ""
#endif

} // mrdocs

#define MRDOCS_DETAIL_PLUGIN_STRINGIZE_I(x) #x
#define MRDOCS_DETAIL_PLUGIN_STRINGIZE(x) MRDOCS_DETAIL_PLUGIN_STRINGIZE_I(x)

#if defined(_DLL) && defined(_DEBUG)
#define MRDOCS_DETAIL_PLUGIN_CRT_TAG "dynamic-debug"
#elif defined(_DLL)
#define MRDOCS_DETAIL_PLUGIN_CRT_TAG "dynamic"
#elif defined(_DEBUG)
#define MRDOCS_DETAIL_PLUGIN_CRT_TAG "static-debug"
#else
#define MRDOCS_DETAIL_PLUGIN_CRT_TAG "static"
#endif

#if defined(_GLIBCXX_DEBUG)
#define MRDOCS_DETAIL_PLUGIN_GLIBCXX_DEBUG_TAG "on"
#else
#define MRDOCS_DETAIL_PLUGIN_GLIBCXX_DEBUG_TAG "off"
#endif

// The value is computed in a helper so that MRDOCS_PLUGIN_STDLIB_TAG has a
// single #define.
#if defined(_MSVC_STL_VERSION)
#define MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE \
    "msvc-stl, crt " MRDOCS_DETAIL_PLUGIN_CRT_TAG \
    ", iterator-debug-level " MRDOCS_DETAIL_PLUGIN_STRINGIZE(_ITERATOR_DEBUG_LEVEL)
#elif defined(_LIBCPP_VERSION)
#define MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE \
    "libc++, abi " MRDOCS_DETAIL_PLUGIN_STRINGIZE(_LIBCPP_ABI_VERSION)
#elif defined(_GLIBCXX_RELEASE)
#define MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE \
    "libstdc++, cxx11-abi " MRDOCS_DETAIL_PLUGIN_STRINGIZE(_GLIBCXX_USE_CXX11_ABI) ", debug " MRDOCS_DETAIL_PLUGIN_GLIBCXX_DEBUG_TAG
#else
#define MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE \
    "unrecognized standard library"
#endif

/** The standard library a plugin was built with.

    A plugin and MrDocs pass C++ objects between them: the plugin
    allocates a generator that MrDocs destroys, and both inline the
    standard library types the API exposes. That holds together only when
    both use the same standard library, configured so that it lays those
    types out the same way. A plugin reports this through
    `mrdocs_plugin_stdlib_tag`, and MrDocs refuses one whose value
    differs from its own. The compiler is not part of the tag. A plugin
    built with another compiler on the same standard library, such as
    Clang against a MrDocs built with GCC on libstdc++, is expected to
    load, because only the library's ABI is checked.

    The text names the library and the settings that matter for it.
    Examples are `libstdc++, cxx11-abi 1, debug off`, `libc++, abi 1`,
    and `msvc-stl, crt dynamic, iterator-debug-level 0`. The runtime is
    `dynamic`, `dynamic-debug`, `static` or `static-debug`.

    The tag holds only the settings that change how the standard library's
    types are laid out, or which heap they allocate from. These are the
    libstdc++ dual ABI and debug mode, the libc++ ABI version, and the MSVC
    iterator debug level and C runtime, release or debug. The tag leaves out
    the release of the library and the compiler. libstdc++ stays compatible
    across releases, and compilers that share a standard library, such as GCC
    and Clang on Linux, lay its types out the same way. Assertion and
    hardening settings change what the inline code checks rather than what it
    operates on. A static C runtime on both sides passes the check, yet gives
    each module a heap of its own, so MrDocs and its plugins have to use the
    DLL runtime.
*/
#define MRDOCS_PLUGIN_STDLIB_TAG MRDOCS_DETAIL_PLUGIN_STDLIB_TAG_VALUE

#endif // MRDOCS_API_PLATFORM_HPP
