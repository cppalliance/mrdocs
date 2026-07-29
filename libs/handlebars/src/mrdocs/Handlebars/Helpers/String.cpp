//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/String.hpp>
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

// Impl fragment of Handlebars.cpp (one translation unit); included within
// `namespace mrdocs { namespace helpers {`. Not a standalone header.

namespace string_helpers_detail {
constexpr auto toupper = [](char c) -> char {
        return static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
    };

constexpr auto toLower = [](char c) -> char {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    };

// The subject string a string helper operates on. When the helper is invoked
// as a block helper, this is the rendered block content. Otherwise it is the
// first argument coerced to a string.
inline std::string
subjectString(dom::Array const& arguments)
{
    dom::Value const options = arguments.back();
    if (dom::Value const fn = options.get("fn"))
    {
        return static_cast<std::string>(fn());
    }
    return std::string(arguments.get(0).getString());
}

// Decode the (subject, chars) arguments shared by strip, lstrip and rstrip.
// The subject is the block content (block helper) or the first argument. The
// chars to trim default to ASCII whitespace and may be overridden by the
// following argument.
inline std::pair<std::string, std::string>
stripArguments(dom::Array const& arguments)
{
    std::string chars = " \t\r\n";
    std::size_t const n = arguments.size();
    dom::Value const options = arguments.back();
    if (dom::Value const fn = options.get("fn"))
    {
        if (n > 1)
        {
            chars = std::string(arguments.get(0).getString());
        }
        return { static_cast<std::string>(fn()), chars };
    }
    if (n > 2)
    {
        chars = std::string(arguments.get(1).getString());
    }
    return { std::string(arguments.get(0).getString()), chars };
}

constexpr auto ljust_fn = [](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fill = secondArg.getString();
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fill = arguments.at(2).getString();
            }
        }
        auto text_size = static_cast<std::int64_t>(res.size());
        while (text_size < width)
        {
            auto const filled_size = text_size + static_cast<std::int64_t>(fill.size());
            if (filled_size > width)
            {
                res.append(fill, 0, width - res.size());
            }
            else
            {
                res.append(fill);
            }
            text_size = static_cast<std::int64_t>(res.size());
        }
        return res;
    };

constexpr auto rjust_fn = [](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        std::string fill = " ";
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fill = secondArg.getString();
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fill = arguments.at(2).getString();
            }
        }
        auto text_size = static_cast<std::int64_t>(res.size());
        while (text_size < width) {
            auto filled_size = static_cast<std::int64_t>(
                res.size() + fill.size());
            if (filled_size > width)
            {
                res.insert(0, fill, 0, width - res.size());
            }
            else
            {
                res.insert(0, fill);
            }
            text_size = static_cast<std::int64_t>(res.size());
        }
        return res;
    };

auto rfind_index_fn = [](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sub;
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
            str = static_cast<std::string>(fn());
            sub = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            sub = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = detail::normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = detail::normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::size_t pos = str.rfind(sub, start);
        bool const notFound = pos == std::string::npos ||
                              static_cast<std::int64_t>(pos) >= end;
        if (notFound)
        {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(pos);
    };

auto is_digits_fn = dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return c >= '0' && c <= '9';
        });
    });

auto to_upper_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        std::ranges::transform(res, res.begin(), toupper);
        return res;
    });

auto to_lower_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        std::ranges::transform(res, res.begin(), toLower);
        return res;
    });

auto join_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        dom::Array arr;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            arr = firstArg.getArray();
        }
        else
        {
            str = firstArg.getString();
            arr = secondArg.getArray();
        }
        std::string res;
        std::size_t const n = arr.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            if (!res.empty())
            {
                res += str;
            }
            auto const& item = arr.at(i);
            res += item.getString();
        }
        return res;
    });

auto strip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        auto [str, chars] = stripArguments(arguments);
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos)
        {
            return std::string();
        }
        std::size_t endpos = str.find_last_not_of(chars);
        return str.substr(pos, endpos - pos + 1);
    });

auto lstrip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        auto [str, chars] = stripArguments(arguments);
        std::size_t pos = str.find_first_not_of(chars);
        if (pos == std::string::npos)
        {
            return std::string();
        }
        return str.substr(pos);
    });

auto rstrip_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        auto [str, chars] = stripArguments(arguments);
        std::size_t pos = str.find_last_not_of(chars);
        if (pos == std::string::npos) {
            return std::string();
        }
        return str.substr(0, pos + 1);
    });

auto split_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1) {
                sep = firstArg.getString();
                if (n > 2)
                {
                    maxsplit = secondArg.getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                sep = secondArg.getString();
                if (n > 3)
                {
                    maxsplit = arguments.at(2).getInteger();
                }
            }
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t sep_len = sep.size();
        while (maxsplit != 0)
        {
            std::size_t next = str.find(sep, pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(pos));
                break;
            }
            res.emplace_back(str.substr(pos, next - pos));
            pos = next + sep_len;
            if (maxsplit > 0)
            {
                --maxsplit;
            }
        }
        return res;
    });

auto slice_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t start = 0;
        std::int64_t stop = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            stop = static_cast<std::int64_t>(res.size());
            start = firstArg.getInteger();
            if (n > 2)
            {
                stop = secondArg.getInteger();
            }
        }
        else
        {
            res = firstArg.getString();
            stop = static_cast<std::int64_t>(res.size());
            start = secondArg.getInteger();
            if (n > 3)
            {
                stop = arguments.at(2).getInteger();
            }
        }
        if (res.empty())
        {
            return std::string();
        }
        start = detail::normalize_index(start, static_cast<std::int64_t>(res.size()));
        stop = detail::normalize_index(stop, static_cast<std::int64_t>(res.size()));
        if (start >= stop)
        {
            return std::string();
        }
        return res.substr(start, stop - start);
    });

void
registerStringHelpers_1(Handlebars& hbs)
{
    hbs.registerHelper("to_json", [](dom::Value const& v) -> dom::Value {
        return dom::JSON::stringify(v);
    });
    hbs.registerHelper("capitalize", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        if (!res.empty())
        {
            res[0] = toupper(res[0]);
        }
        return res;
    }));
    hbs.registerHelper("center", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        char fillchar = ' ';
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
            if (n > 2)
            {
                fillchar = secondArg.getString().get()[0];
            }
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
            if (n > 3)
            {
                fillchar = arguments.at(2).getString().get()[0];
            }
        }
        if (width > static_cast<std::int64_t>(res.size()))
        {
            std::size_t pad = (width - res.size()) / 2;
            res.insert(0, pad, fillchar);
            res.append(pad, fillchar);
        }
        return res;
    }));
    hbs.registerHelper("ljust", dom::makeVariadicInvocable(ljust_fn));
    hbs.registerHelper("pad_end", dom::makeVariadicInvocable(ljust_fn));
    hbs.registerHelper("rjust", dom::makeVariadicInvocable(rjust_fn));}

void
registerStringHelpers_1b(Handlebars& hbs)
{
    hbs.registerHelper("pad_start", dom::makeVariadicInvocable(rjust_fn));
    hbs.registerHelper("count", dom::makeVariadicInvocable(detail::count_fn));
    hbs.registerHelper("ends_with", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string suffix;
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
            str = static_cast<std::string>(fn());
            suffix = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            suffix = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = detail::normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = detail::normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.ends_with(suffix);
    }));
    hbs.registerHelper("starts_with", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string prefix;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        auto const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (!firstArg.isString())
            {
                return false;
            }
            prefix = firstArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 2)
            {
                start = secondArg.getInteger();
                if (n > 3)
                {
                    end = arguments.at(2).getInteger();
                }
            }
        }
        else
        {
            if (!firstArg.isString())
            {
                return false;
            }
            str = firstArg.getString();
            if (!secondArg.isString())
            {
                return false;
            }
            prefix = secondArg.getString();
            end = static_cast<std::int64_t>(str.size());
            if (n > 3)
            {
                start = arguments.at(2).getInteger();
                if (n > 4)
                {
                    end = arguments.at(3).getInteger();
                }
            }
        }
        start = detail::normalize_index(start, static_cast<std::int64_t>(str.size()));
        end = detail::normalize_index(end, static_cast<std::int64_t>(str.size()));
        std::string substr = str.substr(start, end - start);
        return substr.starts_with(prefix);
    }));
}

void
registerStringHelpers_2(Handlebars& hbs)
{
    hbs.registerHelper("expandtabs", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::int64_t tabsize = 8;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                tabsize = firstArg.getInteger();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                tabsize = secondArg.getInteger();
            }
        }
        std::string res;
        res.reserve(str.size());
        for (char c : str)
        {
            if (c == '\t')
            {
                res.append(tabsize, ' ');
            }
            else
            {
                res.push_back(c);
            }
        }
        return res;
    }));
    hbs.registerHelper("find", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("index_of", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("includes", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> bool
    {
        dom::Value res = detail::find_index_fn(arguments);
        if (res.isInteger())
        {
            return res.getInteger() >= 0;
        }
        if (res.isUndefined())
        {
            return false;
        }
        if (res.isString())
        {
            return true;
        }
        return res.isTruthy();
    }));
    hbs.registerHelper("rfind", dom::makeVariadicInvocable(rfind_index_fn));
    hbs.registerHelper("rindex_of", dom::makeVariadicInvocable(rfind_index_fn));
    hbs.registerHelper("last_index_of", dom::makeVariadicInvocable(rfind_index_fn));
    hbs.registerHelper("at", dom::makeInvocable(detail::at_fn));
    hbs.registerHelper("char_at", dom::makeInvocable(detail::at_fn));
    hbs.registerHelper("is_alnum", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    }));
}

void
registerStringHelpers_3(Handlebars& hbs)
{
    hbs.registerHelper("is_alpha", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z');
        });
    }));
    hbs.registerHelper("is_ascii", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            auto uc = static_cast<unsigned char>(c);
            return uc <= 127;
        });
    }));
    hbs.registerHelper("is_decimal", is_digits_fn);
    hbs.registerHelper("is_digit", is_digits_fn);
    hbs.registerHelper("is_lower",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return c >= 'a' && c <= 'z';
        });
    }));
    hbs.registerHelper("is_upper",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return c >= 'A' && c <= 'Z';
        });
    }));}

void
registerStringHelpers_3b(Handlebars& hbs)
{
    hbs.registerHelper("is_printable",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return c >= 32 && c <= 126;
        });
    }));
    hbs.registerHelper("is_space",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        return std::ranges::all_of(res, [](char c) {
            return c == ' ' || (c >= 9 && c <= 13);
        });
    }));
    hbs.registerHelper("is_title",
       dom::makeVariadicInvocable([](dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        bool prev_is_cased = false;
        bool res_is_title = false;
        for (char c : res) {
            if (c >= 'A' && c <= 'Z') {
                if (prev_is_cased) {
                    return false;
                }
                prev_is_cased = true;
                res_is_title = true;
            } else if (c >= 'a' && c <= 'z') {
                if (!prev_is_cased) {
                    return false;
                }
                prev_is_cased = true;
            }
            else
            {
                prev_is_cased = false;
            }
        }
        return res_is_title;
    }));
    hbs.registerHelper("upper", to_upper_fn);
}

void
registerStringHelpers_4(Handlebars& hbs)
{
    hbs.registerHelper("to_upper", to_upper_fn);
    hbs.registerHelper("lower", to_lower_fn);
    hbs.registerHelper("to_lower", to_lower_fn);
    hbs.registerHelper("swap_case", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        if (res.empty()) {
            return res;
        }
        std::ranges::transform(res, res.begin(), [](char c) -> char
        {
            if (c >= 'A' && c <= 'Z')
            {
                return toLower(c);
            }
            else if (c >= 'a' && c <= 'z')
            {
                return toupper(c);
            }
            else
            {
                return c;
            }
        });
        return res;
    }));
    hbs.registerHelper("join", join_fn);
    hbs.registerHelper("implode", join_fn);
    hbs.registerHelper("concat", dom::makeVariadicInvocable(detail::concat_fn));
    hbs.registerHelper("strip", strip_fn);
    hbs.registerHelper("trim", strip_fn);
    hbs.registerHelper("lstrip", lstrip_fn);
}

void
registerStringHelpers_5(Handlebars& hbs)
{
    hbs.registerHelper("trim_start", lstrip_fn);
    hbs.registerHelper("rstrip", rstrip_fn);
    hbs.registerHelper("trim_end", rstrip_fn);
    hbs.registerHelper("partition", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sep = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            sep = secondArg.getString();
        }
        dom::Array res;
        std::size_t pos = str.find(sep);
        if (pos == std::string::npos)
        {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        }
        else
        {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    }));
    hbs.registerHelper("rpartition", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            sep = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            sep = secondArg.getString();
        }
        dom::Array res;
        std::size_t pos = str.rfind(sep);
        if (pos == std::string::npos)
        {
            res.emplace_back(str);
            res.emplace_back(std::string());
            res.emplace_back(std::string());
        }
        else
        {
            res.emplace_back(str.substr(0, pos));
            res.emplace_back(sep);
            res.emplace_back(str.substr(pos + sep.size()));
        }
        return res;
    }));
    hbs.registerHelper("remove_prefix", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string prefix;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            prefix = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            prefix = secondArg.getString();
        }
        if (str.size() < prefix.size())
        {
            return str;
        }
        if (str.substr(0, prefix.size()) != prefix)
        {
            return str;
        }
        return str.substr(prefix.size());
    }));
    hbs.registerHelper("remove_suffix", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string suffix;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            suffix = firstArg.getString();
        }
        else
        {
            str = firstArg.getString();
            suffix = secondArg.getString();
        }
        if (str.size() < suffix.size())
        {
            return str;
        }
        if (str.substr(str.size() - suffix.size()) != suffix)
        {
            return str;
        }
        return str.substr(0, str.size() - suffix.size());
    }));
    hbs.registerHelper("replace", dom::makeVariadicInvocable(detail::replace_fn));
    hbs.registerHelper("split", split_fn);
    hbs.registerHelper("explode", split_fn);
}

void
registerStringHelpers_6(Handlebars& hbs)
{
    hbs.registerHelper("rsplit", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        std::string sep = " ";
        std::int64_t maxsplit = -1;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.get(0);
        dom::Value secondArg = arguments.get(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                sep = firstArg.getString();
                if (n > 2)
                {
                    maxsplit = secondArg.getInteger();
                }
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2) {
                sep = secondArg.getString();
                if (n > 3)
                {
                    maxsplit = arguments.at(2).getInteger();
                }
            }
        }
        dom::Array res;
        std::size_t pos = str.size();
        std::size_t sep_len = sep.size();
        while (maxsplit != 0)
        {
            std::size_t next = str.rfind(sep, pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(0, pos));
                break;
            }
            res.emplace_back(str.substr(next + sep_len, pos - next - sep_len));
            if (next == 0)
            {
                break;
            }
            pos = next - 1;
            if (maxsplit > 0)
            {
                --maxsplit;
            }
        }
        return res;
    }));
    hbs.registerHelper("split_lines", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str;
        bool keepends = false;
        std::size_t const n = arguments.size();
        dom::Value options = arguments.back();
        dom::Value fn = options.get("fn");
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            str = static_cast<std::string>(fn());
            if (n > 1)
            {
                keepends = firstArg.getBool();
            }
        }
        else
        {
            str = firstArg.getString();
            if (n > 2)
            {
                keepends = secondArg.getBool();
            }
        }
        dom::Array res;
        std::size_t pos = 0;
        std::size_t len = str.size();
        while (pos < len)
        {
            std::size_t next = str.find_first_of("\r\n", pos);
            if (next == std::string::npos)
            {
                res.emplace_back(str.substr(pos));
                break;
            }
            if (keepends)
            {
                res.emplace_back(str.substr(pos, next - pos + 1));
            }
            else
            {
                res.emplace_back(str.substr(pos, next - pos));
            }
            pos = next + 1;
        }
        return res;
    }));
    hbs.registerHelper("zfill", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t width = 0;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            width = firstArg.getInteger();
        }
        else
        {
            res = firstArg.getString();
            width = secondArg.getInteger();
        }
        if (width <= static_cast<std::int64_t>(res.size()))
        {
            return res;
        }
        std::string prefix;
        if (res[0] == '+' || res[0] == '-')
        {
            prefix = res[0];
            res = res.substr(1);
            if (width != static_cast<std::int64_t>(res.size()))
                --width;
        }
        std::string padding(width - res.size(), '0');
        return prefix + padding + res;
    }));
    hbs.registerHelper("repeat", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res;
        std::int64_t count = 0;
        dom::Value options = arguments.back();
        dom::Value firstArg = arguments.at(0);
        dom::Value secondArg = arguments.at(1);
        dom::Value fn = options.get("fn");
        bool const isBlock = static_cast<bool>(fn);
        if (isBlock)
        {
            res = static_cast<std::string>(fn());
            count = firstArg.getInteger();
        }
        else
        {
            res = firstArg.getString();
            count = secondArg.getInteger();
        }
        if (count <= 0)
        {
            return std::string();
        }
        std::string tmp;
        while (count > 0)
        {
            tmp += res;
            --count;
        }
        return tmp;
    }));
    hbs.registerHelper("escape", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string str = subjectString(arguments);
        std::string res;
        OutputRef out(res);
        HTMLEscape(out, str);
        return res;
    }));}

void
registerStringHelpers_6b(Handlebars& hbs)
{
    hbs.registerHelper("slice", slice_fn);
    hbs.registerHelper("substr", slice_fn);
    hbs.registerHelper("safe_anchor_id", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        std::ranges::transform(res, res.begin(), [](char c) {
            if (c == ' ' || c == '_')
            {
                return '-';
            }
           return toLower(c);
        });
        // remove any ":" from the string
        auto it = std::remove(res.begin(), res.end(), ':');
        res.erase(it, res.end());
        return res;
    }));
    hbs.registerHelper("strip_namespace", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::string res = subjectString(arguments);
        int inside = 0;
        size_t count = 0;
        size_t offset = std::string::npos;
        for (char c: res)
        {
            switch (c)
            {
            case '(':
            case '[':
            case '<':
            {
                inside++;
                break;
            }
            case ')':
            case ']':
            case '>':
            {
                inside--;
                break;
            }
            case ':':
            {
                if (inside == 0) {
                    offset = count + 1;
                }
                break;
            }
            default:
            {
                break;
            }
            }
            count++;
        }
        if (offset != std::string::npos)
        {
            return res.substr(offset);
        }
        else
        {
            return res;
        }
    }));
}

} // namespace string_helpers_detail

void
registerStringHelpers(Handlebars& hbs)
{
    string_helpers_detail::registerStringHelpers_1(hbs);
    string_helpers_detail::registerStringHelpers_1b(hbs);
    string_helpers_detail::registerStringHelpers_2(hbs);
    string_helpers_detail::registerStringHelpers_3(hbs);
    string_helpers_detail::registerStringHelpers_3b(hbs);
    string_helpers_detail::registerStringHelpers_4(hbs);
    string_helpers_detail::registerStringHelpers_5(hbs);
    string_helpers_detail::registerStringHelpers_6(hbs);
    string_helpers_detail::registerStringHelpers_6b(hbs);
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
