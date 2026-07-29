//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//


#include <mrdocs/Handlebars/Helpers/detail/Sequence.hpp>
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
namespace detail {

dom::Value
at_fn(dom::Value range, dom::Value field, dom::Value options)
{
    auto isBlock = options.isUndefined() && static_cast<bool>(field.get("fn"));
    if (isBlock)
    {
        options = field;
        field = range;
        range = options.get("fn")();
    }

    std::int64_t index = 0;
    if (field.isInteger())
    {
        index = field.getInteger();
    }

    if (range.isString())
    {
        std::string str(range.getString().get());
        index = normalize_index(index, static_cast<std::int64_t>(str.size()));
        return std::string(1, str.at(index));
    }
    else if (range.isArray())
    {
        dom::Array const& arr = range.getArray();
        index = normalize_index(index, static_cast<std::int64_t>(arr.size()));
        return arr.at(index);
    }
    else if (range.isObject())
    {
        dom::Object const& obj = range.getObject();
        if (!field.isString())
        {
            return nullptr;
        }
        std::string key(field.getString().get());
        if (obj.exists(key))
        {
            return obj.get(key);
        }
        return nullptr;
    }
    else
    {
        return range;
    }
}

dom::Expected<dom::Value>
concat_fn(dom::Array const& arguments)
{
    dom::Value options = arguments.back();
    dom::Value fn = options.get("fn");
    auto const isBlock = static_cast<bool>(fn);
    if (isBlock)
    {
        // Block overload: concatenate the contents of the
        // block with the contents of the arguments as strings.
        std::string str = static_cast<std::string>(fn());
        for (std::size_t i = 0; i < arguments.size() - 1; ++i)
        {
            str += toString(arguments.get(i));
        }
        return str;
    }

    // Check if we have at least one argument besides the options
    if (arguments.size() == 1)
    {
        return Unexpected(dom::Error("#concat requires at least one argument"));
    }

    dom::Value firstArg = arguments.get(0);

    // Array overload: concatenate all arguments a single array.
    if (firstArg.isArray())
    {
        dom::Array res;
        for (std::size_t i = 0; i < arguments.size() - 1; ++i)
        {
            dom::Value arg = arguments.get(i);
            if (arg.isArray())
            {
                for (dom::Value item : arg.getArray())
                {
                    res.emplace_back(item);
                }
            }
            else
            {
                res.emplace_back(arg);
            }
        }
        return res;
    }

    // Object overload: concatenate all arguments into a single object.
    if (firstArg.isObject())
    {
        dom::Object res = firstArg.getObject();
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            dom::Value arg = arguments.get(i);
            if (arg.isObject())
            {
                res = createFrame(arg.getObject(), res);
            }
            else
            {
                return Unexpected(dom::Error("All arguments to #concat must be objects"));
            }
        }
        return res;
    }

    // String overload: concatenate all arguments as strings.
    std::string str;
    for (std::size_t i = 0; i < arguments.size() - 1; ++i)
    {
        str += toString(arguments.get(i));
    }
    return str;
}

std::int64_t
count_fn(dom::Array const& arguments)
{
    std::size_t const n = arguments.size();
    dom::Value options = arguments.back();
    dom::Value fn = options.get("fn");
    auto const isBlock = static_cast<bool>(fn);
    dom::Value firstArg = arguments.get(0);
    dom::Value secondArg = arguments.get(1);
    bool const stringOverload =
        (isBlock && firstArg.isString()) ||
        (firstArg.isString() && secondArg.isString());
    if (stringOverload)
    {
        std::string str;
        std::string sub;
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sub = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = arguments.get(1).getInteger();
                if (n > 3)
                {
                    end = arguments.get(2).getInteger();
                }
            }
        }
        else /* !isBlock */
        {
            str = arguments.get(0).getString();
            sub = arguments.get(1).getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.get(2).getInteger();
                if (n > 4)
                {
                    end = arguments.get(3).getInteger();
                }
            }
        }
        start = normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::int64_t count = 0;
        for (std::int64_t pos = start; pos < end; ++pos)
        {
            if (str.compare(pos, sub.size(), sub) == 0)
            {
                ++count;
            }
        }
        return count;
    }
    else
    {
        // Generic range overload
        dom::Value range = arguments.get(0);
        dom::Value item = arguments.get(1);
        if (range.isString())
        {
            // String overload: find chars in it
            std::string str(range.getString().get());
            char x = item.getString().get().at(0);
            return std::ranges::count(str, x);
        }
        else if (range.isArray())
        {
            // Array overload: find items in it
            dom::Array const& arr = range.getArray();
            return std::ranges::count(arr, item);
        }
        else if (range.isObject())
        {
            // Object overload: find values in it
            dom::Object const& obj = range.getObject();
            std::int64_t count = 0;
            obj.visit([&](dom::String const&, dom::Value const& val) {
                if (val == item)
                {
                    ++count;
                }
            });
            return count;
        }
        else
        {
            // Generic overload: return 0
            return 0;
        }
    }
}

dom::Value
replace_fn(dom::Array const& arguments)
{
    std::size_t const n = arguments.size();
    dom::Value options = arguments.back();
    dom::Value fn = options.get("fn");
    dom::Value firstArg = arguments.get(0);
    dom::Value secondArg = arguments.get(1);
    bool const isBlock = static_cast<bool>(fn);
    bool const stringOverload =
        (isBlock && firstArg.isString()) ||
        (firstArg.isString() && secondArg.isString());
    if (stringOverload)
    {
        // String overload: replace substrings
        std::string str;
        std::string old;
        std::string new_str;
        std::int64_t count = -1;
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            old = firstArg.getString();
            new_str = secondArg.getString();
            if (n > 3)
            {
                count = arguments.at(2).getInteger();
            }
        }
        else
        {
            str = firstArg.getString();
            old = secondArg.getString();
            new_str = arguments.at(2).getString();
            if (n > 4)
            {
                count = arguments.at(3).getInteger();
            }
        }
        std::string res;
        std::size_t pos = 0;
        std::size_t old_len = old.size();
        while (count != 0)
        {
            std::size_t next = str.find(old, pos);
            if (next == std::string::npos)
            {
                res += str.substr(pos);
                break;
            }
            res += str.substr(pos, next - pos);
            res += new_str;
            pos = next + old_len;
            if (count > 0)
            {
                --count;
            }
        }
        return res;
    }
    // Generic range overload
    dom::Value range = firstArg;
    dom::Value const& item = secondArg;
    dom::Value replacement = arguments.at(2);
    if (range.isString())
    {
        // String overload: replace chars in it
        std::string str(range.getString().get());
        auto str_n = static_cast<std::int64_t>(str.size());
        for (std::int64_t i = 0; i < str_n; ++i)
        {
            if (str.at(i) == item.getString().get().at(0))
            {
                str.replace(
                    str.begin() + i,
                    str.begin() + i + 1,
                    replacement.getString().get());
                std::size_t n2 = replacement.getString().get().size();
                i += static_cast<std::int64_t>(n2) - 1;
            }
        }
        return str;
    }
    else if (range.isArray())
    {
        // Array overload: replace items in it
        dom::Array const& arr = range.getArray();
        dom::Array res;
        std::size_t arr_n = arr.size();
        for (std::size_t i = 0; i < arr_n; ++i)
        {
            dom::Value v = arr.at(i);
            if (v == item)
            {
                res.emplace_back(replacement);
            }
            else
            {
                res.emplace_back(v);
            }
        }
        return res;
    }
    else if (range.isObject())
    {
        // Object overload: replace values in it
        dom::Object obj = createFrame(range.getObject());
        obj.visit([&](dom::String const& key, dom::Value const& val) {
            if (val == item)
            {
                obj.set(key, replacement);
            }
        });
        return obj;
    }
    else
    {
        // Generic overload: replace nothing
        return range;
    }
}

dom::Value
find_index_fn(
    dom::Array const& arguments)
{
    dom::Value range;
    dom::Value val;
    std::int64_t start = 0;
    std::int64_t end = 0;

    std::size_t const n = arguments.size();
    dom::Value options = arguments.back();
    dom::Value fn = options.get("fn");
    dom::Value firstArg = arguments.get(0);
    dom::Value secondArg = arguments.get(1);

    bool const isBlock = static_cast<bool>(fn);
    if (isBlock)
    {
        // val, start, end
        range = static_cast<std::string>(fn());
        val = firstArg.getString();
        end = static_cast<std::int64_t>(range.size());
        if (n > 2)
        {
            start = secondArg.getInteger();
            if (n > 3)
            {
                end = arguments.get(2).getInteger();
            }
        }
    }
    else
    {
        // range, val, start, end
        range = firstArg;
        val = secondArg;
        end = static_cast<std::int64_t>(range.size());
        if (n > 3)
        {
            start = arguments.at(2).getInteger();
            if (n > 4)
            {
                end = arguments.at(3).getInteger();
            }
        }
    }
    start = normalize_index(start, static_cast<std::int64_t>(range.size()));
    end = normalize_index(end, static_cast<std::int64_t>(range.size()));
    if (range.isString())
    {
        // Find position of substring val in range
        std::size_t pos = range.getString().get().find(val.getString(), start);
        if (pos == std::string::npos || static_cast<std::int64_t>(pos) >= end) {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(pos);
    }
    if (range.isArray())
    {
        // Find position of item val in array
        auto const& arr = range.getArray();
        for (std::int64_t i = start; i < end; i++)
        {
            if (arr.get(i) == val)
            {
                return i;
            }
        }
        return static_cast<std::int64_t>(-1);
    }
    else if (range.isObject())
    {
        // Find key of item val in object
        auto const& obj = range.getObject();
        dom::Value res(dom::Kind::Undefined);
        auto i = 0;
        obj.visit([&](dom::String const& k, dom::Value const& v) -> bool
        {
            if (i < start)
            {
                ++i;
                return true;
            }
            if (i >= end)
            {
                return false;
            }
            if (v == val)
            {
                res = k;
                return false;
            }
            return true;
        });
        return res;
    }
    return range;
}

} // namespace detail
} // namespace handlebars
} // namespace mrdocs
