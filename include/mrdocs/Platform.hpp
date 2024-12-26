//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_PLATFORM_HPP
#define MRDOCS_API_PLATFORM_HPP

#include <type_traits>

#if __cplusplus < 202002L
    #error "mrdocs requires at least C++20"
#endif

/*
    Platform-specific things, and stuff
    that is dependent on the toolchain.
*/

namespace clang {
namespace mrdocs {

/** The minimum version of LLVM required
*/
#define MRDOCS_MINIMUM_LLVM_VERSION 15

//------------------------------------------------
//
// Shared Libraries
//
//------------------------------------------------

// static linking
#if defined(MRDOCS_STATIC_LINK)
# define MRDOCS_DECL
# define MRDOCS_VISIBLE

// MSVC
#elif defined(_MSC_VER)
# define MRDOCS_SYMBOL_EXPORT __declspec(dllexport)
# define MRDOCS_SYMBOL_IMPORT __declspec(dllimport)
# if defined(MRDOCS_TOOL) // building tool
#  define MRDOCS_DECL MRDOCS_SYMBOL_EXPORT
# else
#  define MRDOCS_DECL MRDOCS_SYMBOL_IMPORT
# endif
# define MRDOCS_VISIBLE

// (unknown)
#elif defined(__GNUC__)
# if defined(MRDOCS_TOOL) // building library
#   define MRDOCS_DECL
# else
#   define MRDOCS_DECL __attribute__((__visibility__("default")))
#endif
# define MRDOCS_VISIBLE __attribute__((__visibility__("default")))
#else
# error unknown platform for dynamic linking
#endif

//------------------------------------------------

#ifndef FMT_CONSTEVAL
# if !defined(__GNUC__) && defined(_MSC_VER)
#  define FMT_CONSTEVAL
# endif
#endif

#if ! defined(__x86_64__) && ! defined(_WIN64) && ! defined(__aarch64__)
# error mrdocs requires a 64-bit architecture
#endif

} // mrdocs
} // clang

#include <mrdocs/Support/Assert.hpp>

#endif
