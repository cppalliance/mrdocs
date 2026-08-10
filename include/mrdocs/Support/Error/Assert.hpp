//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ERROR_ASSERT_HPP
#define MRDOCS_API_SUPPORT_ERROR_ASSERT_HPP

#include <mrdocs/Support/Export.hpp>
#include <cstdint>

/** Core MrDocs support utilities.

    The public `mrdocs` namespace here centralizes assert/assume hooks so we can
    swap behavior (terminate, throw, debugbreak) in one place without leaking
    platform specifics into the rest of the codebase.
*/
namespace mrdocs {

#ifdef NDEBUG
    #ifdef __GNUC__
        #define MRDOCS_UNREACHABLE() static_cast<void>(__builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOCS_UNREACHABLE() static_cast<void>(__assume(false))
    #endif
    #define MRDOCS_ASSERT(x) static_cast<void>(false)
#else
    #ifdef __GNUC__
        #define MRDOCS_UNREACHABLE() static_cast<void>(__builtin_trap(), __builtin_unreachable())
    #elif defined(_MSC_VER)
        #define MRDOCS_UNREACHABLE() static_cast<void>(__debugbreak(), __assume(false))
    #endif

    /** Handler invoked when `MRDOCS_ASSERT` fails.

        @param msg Expression string that failed.
        @param file Source file where the assertion triggered.
        @param line Line within the source file.
    */
    MRDOCS_DECL
    void
    assert_failed(
        char const* msg,
        char const* file,
        std::uint_least32_t line);

    #define MRDOCS_ASSERT(x) static_cast<void>(!! (x) || \
        (assert_failed(#x, __builtin_FILE(), __builtin_LINE()), \
        MRDOCS_UNREACHABLE(), true))
#endif

} // mrdocs

#endif
