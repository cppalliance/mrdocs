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

/** Formats a duration into a human-readable string.

    This function takes a `std::chrono::duration` object and formats it into a
    string that represents the duration in a human-readable format. The format
    includes hours, minutes, seconds, and milliseconds as appropriate.

    @param delta The duration to format.
    @return A string representing the formatted duration.
 */
template <class Rep, class Period>
std::string
format_duration(
    std::chrono::duration<Rep, Period> delta)
{
    auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
    if (delta < std::chrono::seconds(1))
    {
        return fmt::format("{}ms", delta_ms.count());
    }

    auto delta_s = std::chrono::duration_cast<std::chrono::seconds>(delta);
    if (delta < std::chrono::minutes(1))
    {
        delta_ms -= delta_s;
        return fmt::format("{}s {}ms", delta_s.count(), delta_ms.count());
    }

    auto delta_min = std::chrono::duration_cast<std::chrono::minutes>(delta);
    if (delta < std::chrono::hours(1))
    {
        delta_ms -= delta_s;
        delta_s -= delta_min;
        return fmt::format("{}min {}s {}ms", delta_min.count(), delta_s.count(), delta_ms.count());
    }

    auto delta_h = std::chrono::duration_cast<std::chrono::hours>(delta);
    delta_ms -= delta_s;
    delta_s -= delta_min;
    delta_min -= delta_h;
    return fmt::format("{}h {}min {}s {}ms", delta_h.count(), delta_min.count(), delta_s.count(), delta_ms.count());
}

} // mrdocs
} // clang

#endif
