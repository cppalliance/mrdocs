//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/Constructor.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Handlebars/Helpers/detail/KeyPath.hpp>
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
// Value-constructor helpers (str, arr, ...).

void
registerConstructorHelpers(Handlebars& hbs)
{
    // A helper that constructs a string from the first argument.
    hbs.registerHelper("str", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Expected<dom::Value>
    {
        if (arguments.size() != 2)
        {
            return Unexpected(dom::Error("#str requires exactly one argument"));
        }
        return arguments.at(0);
    }));

    // A helper that constructs an array from the arguments.
    hbs.registerHelper("arr", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        dom::Array res;
        for (std::size_t i = 0; i < arguments.size() - 1; ++i)
        {
            res.emplace_back(arguments.at(i));
        }
        return res;
    }));

    // A helper that constructs an array of indexes.
    hbs.registerHelper("range", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        std::int64_t start = 0;
        std::int64_t stop = 0;
        std::int64_t step = 1;
        std::size_t const n = arguments.size() - 1;
        if (n == 1)
        {
            stop = arguments.at(0).getInteger();
        }
        else if (n == 2)
        {
            start = arguments.at(0).getInteger();
            stop = arguments.at(1).getInteger();
        }
        else if (n == 3)
        {
            start = arguments.at(0).getInteger();
            stop = arguments.at(1).getInteger();
            step = arguments.at(2).getInteger();
        }
        dom::Array res;
        if (step > 0)
        {
            for (std::int64_t i = start; i < stop; i += step)
            {
                res.emplace_back(i);
            }
        }
        else
        {
            for (std::int64_t i = start; i > stop; i += step)
            {
                res.emplace_back(i);
            }
        }
        return res;
    }));
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
