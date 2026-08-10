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

#if __cplusplus < 202002L
    #error "mrdocs requires at least C++20"
#endif

/*
    Platform-specific things, and stuff
    that is dependent on the toolchain.
*/


namespace mrdocs {

/** The minimum version of LLVM required
*/
#define MRDOCS_MINIMUM_LLVM_VERSION 15

//------------------------------------------------

#ifndef FMT_CONSTEVAL
# if !defined(__GNUC__) && defined(_MSC_VER)
#  define FMT_CONSTEVAL
# endif
#endif

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


#endif // MRDOCS_API_PLATFORM_HPP
