//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_PLATFORM_HPP
#define MRDOX_PLATFORM_HPP

/*
    Platform-specific things, and stuff
    that is dependent on the toolchain.
*/

namespace clang {
namespace mrdox {

// MSVC
#if defined(_MSC_VER)
# define MRDOX_SYMBOL_EXPORT __declspec(dllexport)
# define MRDOX_SYMBOL_IMPORT __declspec(dllimport)
# define MRDOX_SYMBOL_VISIBLE
# if defined(MRDOX_LIB) // building library
#  define MRDOX_DECL MRDOX_SYMBOL_EXPORT
# else
#  define MRDOX_DECL MRDOX_SYMBOL_IMPORT
# endif

// (others)
#else
# define MRDOX_SYMBOL_EXPORT
# define MRDOX_SYMBOL_IMPORT
# define MRDOX_SYMBOL_VISIBLE
# define MRDOX_DECL
#endif

} // mrdox
} // clang

#endif
