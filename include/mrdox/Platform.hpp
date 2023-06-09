//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_PLATFORM_HPP
#define MRDOX_API_PLATFORM_HPP

#include <type_traits>

#if __cplusplus < 202002L
    #error "mrdox requires at least C++20"
#endif

/*
    Platform-specific things, and stuff
    that is dependent on the toolchain.
*/

namespace clang {
namespace mrdox {

/** The minimum version of LLVM required
*/
#define MRDOX_MINIMUM_LLVM_VERSION 15

//------------------------------------------------
//
// Shared Libraries
//
//------------------------------------------------

// MSVC
#if defined(_MSC_VER)
# define MRDOX_SYMBOL_EXPORT __declspec(dllexport)
# define MRDOX_SYMBOL_IMPORT __declspec(dllimport)
# if defined(MRDOX_TOOL) // building tool
#  define MRDOX_DECL MRDOX_SYMBOL_EXPORT
# else
#  define MRDOX_DECL MRDOX_SYMBOL_IMPORT
# endif
# define MRDOX_VISIBLE

// (unknown)
#elif defined(__GNUC__)
#  define MRDOX_DECL __attribute__((visibility("default")))
#  define MRDOX_VISIBLE __attribute__((visibility("default")))
#else
# error unknown platform for dynamic linking
#endif



} // mrdox
} // clang

#endif
