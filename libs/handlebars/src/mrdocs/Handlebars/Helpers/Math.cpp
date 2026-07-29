//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/Math.hpp>
#include <mrdocs/Handlebars.hpp>
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
// Arithmetic helpers (add, sub, mul, div, ...).

void
registerMathHelpers(Handlebars& hbs)
{
    hbs.registerHelper("add", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Expected<dom::Value>
    {
        if (arguments.size() < 2)
        {
            return Unexpected(dom::Error("add requires at least two arguments"));
        }
        if (!arguments.at(0).isInteger())
        {
            return Unexpected(dom::Error("add requires integer arguments"));
        }
        std::int64_t res = arguments.at(0).getInteger();
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            if (!arguments.at(i).isInteger())
            {
                return Unexpected(dom::Error("add requires integer arguments"));
            }
            res += arguments.at(i).getInteger();
        }
        return res;
    }));

    hbs.registerHelper("sub", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Expected<dom::Value>
    {
        if (arguments.size() < 2)
        {
            return Unexpected(dom::Error("sub requires at least two arguments"));
        }
        if (!arguments.at(0).isInteger())
        {
            return Unexpected(dom::Error("sub requires integer arguments"));
        }
        std::int64_t res = arguments.at(0).getInteger();
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            if (!arguments.at(i).isInteger())
            {
                return Unexpected(dom::Error("sub requires integer arguments"));
            }
            res -= arguments.at(i).getInteger();
        }
        return res;
    }));

    hbs.registerHelper("mul", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Expected<dom::Value>
    {
        if (arguments.size() < 2)
        {
            return Unexpected(dom::Error("mul requires at least two arguments"));
        }
        if (!arguments.at(0).isInteger())
        {
            return Unexpected(dom::Error("mul requires integer arguments"));
        }
        std::int64_t res = arguments.at(0).getInteger();
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            if (!arguments.at(i).isInteger())
            {
                return Unexpected(dom::Error("mul requires integer arguments"));
            }
            res *= arguments.at(i).getInteger();
        }
        return res;
    }));

    hbs.registerHelper("div", dom::makeVariadicInvocable([](
            dom::Array const& arguments) -> dom::Expected<dom::Value>
    {
        if (arguments.size() < 2)
        {
            return Unexpected(dom::Error("div requires at least two arguments"));
        }
        if (!arguments.at(0).isInteger())
        {
            return Unexpected(dom::Error("div requires integer arguments"));
        }
        std::int64_t res = arguments.at(0).getInteger();
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            if (!arguments.at(i).isInteger())
            {
                return Unexpected(dom::Error("div requires integer arguments"));
            }
            res /= arguments.at(i).getInteger();
        }
        return res;
    }));
}

void
registerTypeHelpers(Handlebars& hbs)
{
    hbs.registerHelper("is_string", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isString();
    }));

    hbs.registerHelper("is_array", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isArray();
    }));

    hbs.registerHelper("is_object", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isObject();
    }));

    hbs.registerHelper("is_number", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isInteger();
    }));

    hbs.registerHelper("is_boolean", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isBoolean();
    }));

    hbs.registerHelper("is_null", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isNull();
    }));

    hbs.registerHelper("is_undefined", dom::makeInvocable([](
        dom::Value const& val) -> dom::Value
    {
        return val.isUndefined();
    }));
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
