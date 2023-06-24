//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_STRING_HPP
#define MRDOX_API_SUPPORT_STRING_HPP

#include <mrdox/Platform.hpp>
#include <string>
#include <string_view>
#include <cctype>


namespace clang {
namespace mrdox {

inline
std::string_view
ltrim(std::string_view s)
{
    auto it = s.begin();
    for (;; ++it)
    {
        if (it == s.end())
            return {};
        if (! std::isspace(*it))
            break;
    }
    return s.substr(it - s.begin());
}

inline
std::string_view
rtrim(std::string_view s)
{
    auto it = s.end() - 1;
    for (; it > s.begin() && std::isspace(*it); --it);
    return s.substr(0, it - s.begin());
}

inline
std::string_view
trim(std::string_view s)
{
    auto left = s.begin();
    for (;; ++left)
    {
        if (left == s.end())
            return {};
        if (! std::isspace(*left))
            break;
    }
    auto right = s.end() - 1;
    for (; right > left && std::isspace(*right); --right);
    return std::string_view(&*left, right - left + 1);
}

template<typename Range>
std::string
join(
    Range&& range,
    std::string_view delim)
{
    std::string result;
    if(std::empty(range))
        return result;
    auto first = std::begin(range);
    auto last = std::end(range);
    result.append(*first++);
    while(first != last)
    {
        result.append(delim);
        result.append(*first++);
    }
    return result;
}

} // mrdox
} // clang

#endif
