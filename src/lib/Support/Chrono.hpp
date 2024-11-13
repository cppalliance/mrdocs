//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_CHRONO_HPP
#define MRDOCS_LIB_SUPPORT_CHRONO_HPP

#include <chrono>
#include <string>
#include <fmt/format.h>

namespace clang {
namespace mrdocs {

template <class Rep, class Period>
std::string
format_duration(
    std::chrono::duration<Rep, Period> delta)
{
    auto delta_ms = std::chrono::duration_cast<
                        std::chrono::milliseconds>(delta).count();
    if (delta_ms < 1000)
    {
        return fmt::format("{} ms", delta_ms);
    }
    else
    {
        double const delta_s = static_cast<double>(delta_ms) / 1000.0;
        return fmt::format("{:.02f} s", delta_s);
    }
}

} // mrdocs
} // clang

#endif
