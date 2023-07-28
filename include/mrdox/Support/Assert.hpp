//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_ASSERT_HPP
#define MRDOX_API_SUPPORT_ASSERT_HPP

#include <cstdint>

namespace clang {
namespace mrdox {

#ifdef NDEBUG
    #ifdef __GNUC__
        #define MRDOX_UNREACHABLE() static_cast<void>(__builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOX_UNREACHABLE() static_cast<void>(__assume(false))
    #endif
    #define MRDOX_ASSERT(x) static_cast<void>(false)
#else
    #ifdef __GNUC__
        #define MRDOX_UNREACHABLE() static_cast<void>(__builtin_trap(), __builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOX_UNREACHABLE() static_cast<void>(__debugbreak(), __assume(false))
    #endif

    void
    assert_failed(
        const char* msg,
        const char* file,
        std::uint_least32_t line);

    #define MRDOX_ASSERT(x) static_cast<void>(!! (x) || \
        (assert_failed(#x, __builtin_FILE(), __builtin_LINE()), \
        MRDOX_UNREACHABLE(), true))
#endif

} // mrdox
} // clang

#endif