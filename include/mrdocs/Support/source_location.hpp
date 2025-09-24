//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_SOURCE_LOCATION_HPP
#define MRDOCS_API_SUPPORT_SOURCE_LOCATION_HPP

#include <version>

#if __cpp_lib_source_location >= 201907L && \
    __has_include(<source_location>)
#    include <source_location>

    namespace clang {
    namespace mrdocs {

    using std::source_location;

    } // mrdocs
    } // clang
#else
#    include <cstdint>

    namespace clang {
    namespace mrdocs {

    struct source_location
    {
        static
        constexpr
        source_location
        current(
            char const* const file = __builtin_FILE(),
            char const* const function = __builtin_FUNCTION(),
            const std::uint_least32_t line = __builtin_LINE(),
            const std::uint_least32_t column = __builtin_COLUMN()) noexcept

        {
            source_location result;
            result.file_ = file;
            result.function_ = function;
            result.line_ = line;
            result.column_ = column;
            return result;
        }

        constexpr
        char const*
        file_name() const noexcept
        {
            return file_;
        }
        constexpr
        char const*
        function_name() const noexcept
        {
            return function_;
        }

        constexpr
        std::uint_least32_t
        line() const noexcept
        {
            return line_;
        }
        constexpr
        std::uint_least32_t
        column() const noexcept
        {
            return column_;
        }

    private:
        char const* file_ = "";
        char const* function_ = "";
        std::uint_least32_t line_ = 0;
        std::uint_least32_t column_ = 0;
    };

} // mrdocs
} // clang
#endif
#endif
