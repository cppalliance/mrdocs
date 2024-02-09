//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Glob.hpp"

namespace clang {
namespace mrdocs {

bool
globMatch(
    std::string_view pattern,
    std::string_view str) noexcept
{
    if (pattern.empty())
    {
        return str.empty();
    }
    if (pattern[0] == '*')
    {
        if (pattern.size() == 1)
        {
            return true;
        }
        for (std::size_t i = 0; i <= str.size(); ++i)
        {
            if (globMatch(pattern.substr(1), str.substr(i)))
            {
                return true;
            }
        }
        return false;
    }
    if (str.empty())
    {
        return false;
    }
    if (pattern[0] == str[0] || pattern[0] == '?')
    {
        return globMatch(pattern.substr(1), str.substr(1));
    }
    return false;
}

} // mrdocs
} // clang
