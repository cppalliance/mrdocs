//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_FORMAT_HPP
#define MRDOX_API_SUPPORT_FORMAT_HPP

#include <mrdox/Platform.hpp>
#include <fmt/format.h>
#include <source_location>

namespace clang {
namespace mrdox {

template<class... Args>
struct FormatString
{
    template<class T>
    FormatString(
        T const& fs_,
        std::source_location loc_ =
            std::source_location::current())
        : fs(fs_)
        , loc(loc_)
    {
        static_assert(std::is_constructible_v<
            std::string_view, T const&>);
    }

    std::string_view fs;
    std::source_location loc;
};

} // mrdox
} // clang

#endif
