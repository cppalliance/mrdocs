//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/CliOverride.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <charconv>
#include <vector>

namespace mrdocs {

namespace {

// Split a dotted path into its segments, e.g. "a.b.c" -> {"a", "b", "c"}.
std::vector<std::string_view>
splitDots(std::string_view path)
{
    std::vector<std::string_view> parts;
    for (;;)
    {
        auto const pos = path.find('.');
        if (pos == std::string_view::npos)
        {
            parts.push_back(path);
            return parts;
        }
        parts.push_back(path.substr(0, pos));
        path.remove_prefix(pos + 1);
    }
}

// A "--key=value" / "--key" token split into its parts (without the "--").
struct ParsedArg
{
    std::string_view key;
    std::string_view value;
    bool hasValue;
};

ParsedArg
parseArg(std::string_view arg)
{
    arg.remove_prefix(2); // caller guarantees a leading "--"
    auto const eq = arg.find('=');
    if (eq == std::string_view::npos)
    {
        return {arg, {}, false};
    }
    return {arg.substr(0, eq), arg.substr(eq + 1), true};
}

// Set `path` (non-empty) within `obj` to `value`, creating intermediate
// objects as needed. dom::Object is a handle to shared storage, so set()
// mutates the entry stored in the caller's map.
void
setNested(
    dom::Object const& obj,
    std::span<std::string_view const> path,
    dom::Value value)
{
    if (path.size() == 1)
    {
        obj.set(std::string(path.front()), std::move(value));
        return;
    }
    dom::Value const child = obj.get(path.front());
    dom::Object const childObj = child.isObject() ? child.getObject() : dom::Object();
    setNested(childObj, path.subspan(1), std::move(value));
    obj.set(std::string(path.front()), childObj);
}

} // (anon)

dom::Value
parseCliScalarValue(std::string_view value)
{
    if (value == "true")
    {
        return dom::Value(true);
    }
    if (value == "false")
    {
        return dom::Value(false);
    }
    if (!value.empty())
    {
        std::int64_t n = 0;
        auto const* const last = value.data() + value.size();
        auto const [ptr, ec] = std::from_chars(value.data(), last, n);
        if (ec == std::errc() && ptr == last)
        {
            return dom::Value(n);
        }
    }
    return dom::Value(std::string(value));
}

bool
isDottedObjectOverride(
    std::string_view arg,
    std::span<std::string_view const> objectOptionNames)
{
    if (!arg.starts_with("--"))
    {
        return false;
    }
    auto const [key, value, hasValue] = parseArg(arg);
    auto const dot = key.find('.');
    if (dot == std::string_view::npos)
    {
        return false;
    }
    std::string_view const head = key.substr(0, dot);
    for (std::string_view const name : objectOptionNames)
    {
        if (name == head)
        {
            return true;
        }
    }
    return false;
}

Expected<void>
applyDottedObjectOverrides(
    std::map<std::string, dom::Object>& target,
    std::string_view optionName,
    char const** argv)
{
    for (char const** p = argv; *p != nullptr; ++p)
    {
        std::string_view const arg(*p);
        if (!arg.starts_with("--"))
        {
            continue;
        }
        auto const [key, value, hasValue] = parseArg(arg);

        // Match "<optionName>.<rest>"; anything else is not ours.
        if (!key.starts_with(optionName) ||
            key.size() <= optionName.size() ||
            key[optionName.size()] != '.')
        {
            continue;
        }
        std::string_view const rest = key.substr(optionName.size() + 1);

        std::vector<std::string_view> const segments = splitDots(rest);
        if (segments.size() < 2)
        {
            return Unexpected(formatError(
                "`--{}` override needs a key and at least one field "
                "(for example `--{}.<key>.<field>=<value>`)",
                key, optionName));
        }
        if (!hasValue)
        {
            return Unexpected(formatError(
                "`--{}` override is missing `=<value>`", key));
        }

        dom::Object& entry = target[std::string(segments.front())];
        setNested(
            entry,
            std::span<std::string_view const>(segments).subspan(1),
            parseCliScalarValue(value));
    }
    return {};
}

} // mrdocs
