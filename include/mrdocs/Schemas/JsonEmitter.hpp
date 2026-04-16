//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SCHEMAS_JSONEMITTER_HPP
#define MRDOCS_API_SCHEMAS_JSONEMITTER_HPP

#include <mrdocs/Dom/Value.hpp>
#include <cstddef>
#include <string>
#include <string_view>

namespace mrdocs::schema {

/** Serialize a dom::Value tree to formatted JSON.

    Handles null, boolean, integer, string, array, and object.

    @param v The value to serialize.
    @param indent Current indentation level in units of two spaces.
    Callers should pass `0` for the top-level call; recursive calls
    increment it for nested arrays and objects.
    @return The JSON text. Arrays and objects are pretty-printed
    with two-space indentation; scalars are emitted without
    surrounding whitespace.
*/
inline
std::string
toJson(dom::Value const& v, int indent = 0)
{
    std::string result;
    auto const pad = [&](int level) -> std::string {
        return std::string(level * 2, ' ');
    };

    switch (v.kind())
    {
    case dom::Kind::Undefined:
    case dom::Kind::Null:
        result = "null";
        break;

    case dom::Kind::Boolean:
        result = v.getBool() ? "true" : "false";
        break;

    case dom::Kind::Integer:
        result = std::to_string(v.getInteger());
        break;

    case dom::Kind::String:
    case dom::Kind::SafeString:
    {
        // Escape special characters.
        result = "\"";
        for (char c : v.getString().get())
        {
            switch (c)
            {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
            }
        }
        result += '"';
        break;
    }

    case dom::Kind::Array:
    {
        auto const& arr = v.getArray();
        if (arr.empty())
        {
            result = "[]";
            break;
        }
        result = "[\n";
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            result += pad(indent + 1);
            result += toJson(arr.at(i), indent + 1);
            if (i + 1 < arr.size())
            {
                result += ',';
            }
            result += '\n';
        }
        result += pad(indent);
        result += ']';
        break;
    }

    case dom::Kind::Object:
    {
        auto const& obj = v.getObject();
        if (obj.empty())
        {
            result = "{}";
            break;
        }
        result = "{\n";
        bool first = true;
        obj.visit([&](dom::String key, dom::Value const& val) -> bool
        {
            if (!first)
            {
                result += ",\n";
            }
            first = false;
            result += pad(indent + 1);
            result += '"';
            result += std::string_view(key);
            result += "\": ";
            result += toJson(val, indent + 1);
            return true;
        });
        result += '\n';
        result += pad(indent);
        result += '}';
        break;
    }

    default:
        result = "null";
        break;
    }

    return result;
}

} // mrdocs::schema

#endif // MRDOCS_API_SCHEMAS_JSONEMITTER_HPP
