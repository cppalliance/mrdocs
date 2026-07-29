//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/Logical.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Handlebars/Helpers/detail/Sequence.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mrdocs {
namespace handlebars {
namespace helpers {

// This .ipp fragment is included from Handlebars.cpp within
// `namespace mrdocs { namespace handlebars { namespace helpers {`. Not a
// standalone header: it has no include guard and is compiled as part of the
// Handlebars translation unit.
//
// Logical/comparison helpers (eq, and, or, not, ...).

void
registerLogicalHelpers(Handlebars& hbs)
{
    hbs.registerHelper("and", dom::makeVariadicInvocable(and_fn));
    hbs.registerHelper("eq", dom::makeVariadicInvocable(eq_fn));
    hbs.registerHelper("gt", dom::makeVariadicInvocable(gt_fn));
    hbs.registerHelper("ne", dom::makeVariadicInvocable(ne_fn));
    hbs.registerHelper("not", dom::makeVariadicInvocable(not_fn));
    hbs.registerHelper("or", dom::makeVariadicInvocable(or_fn));
    hbs.registerHelper("select", dom::makeInvocable(select_fn));
}

bool
and_fn(dom::Array const& args)
{
    std::size_t const n = args.size();
    if (n == 0) return true;
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (!args.get(i))
        {
            return false;
        }
    }
    return true;
}

dom::Value
or_fn(dom::Array const& args)
{
    std::size_t const n = args.size();
    if (n == 0)
    {
        return false;
    }
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (args.get(i))
        {
            return args.get(i);
        }
    }
    return false;
}

bool
eq_fn(dom::Array const& args) {
    if (args.empty())
    {
        return true;
    }
    dom::Value first = args.get(0);
    std::size_t const n = args.size();
    for (std::size_t i = 1; i < n - 1; ++i)
    {
        if (first != args.get(i))
        {
            return false;
        }
    }
    return true;
}

bool
ne_fn(dom::Array const& args) {
    return !eq_fn(args);
}

// Greater-than comparison via `operator<=>` on `dom::Value`.
bool
gt_fn(dom::Array const& args) {
    // args carries trailing options, so we need at least two real
    // arguments plus that one.
    if (args.size() < 3)
    {
        return false;
    }
    return args.get(0) > args.get(1);
}

bool
not_fn(dom::Array const& args) {
    std::size_t const n = args.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        if (!args.get(i))
        {
            return true;
        }
    }
    return false;
}

dom::Value
increment_fn(dom::Value const& value)
{
    if (value)
    {
        return value + 1;
    }
    return 1;
}

dom::Value
select_fn(
    dom::Value const& condition,
    dom::Value const& result_true,
    dom::Value const& result_false)
{
    return isEmpty(condition) ?
        result_false : result_true;
}

dom::Value
detag_fn(dom::Value html)
{
    if (!html)
    {
        return html;
    }

    // remove instances of "/<[^>]+>/g"
    std::string result;
    result.reserve(html.size());
    bool insideTag = false;
    for (char c: html.getString().get())
    {
        if (c == '<')
        {
            insideTag = true;
            continue;
        }
        if (c == '>')
        {
            insideTag = false;
            continue;
        }
        if (!insideTag)
        {
            result.push_back(c);
        }
    }
    return result;
}

int
year_fn() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    return localTime->tm_year + 1900;
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
