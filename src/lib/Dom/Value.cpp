//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Value.hpp>
#include <unordered_set>

namespace clang {
namespace mrdox {
namespace dom {

Value::
~Value()
{
    switch(kind_)
    {
    case Kind::Null:
    case Kind::Boolean:
    case Kind::Integer:
        break;
    case Kind::String:
        std::destroy_at(&str_);
        break;
    case Kind::Array:
        std::destroy_at(&arr_);
        break;
    case Kind::Object:
        std::destroy_at(&obj_);
        break;
    case Kind::Function:
        std::destroy_at(&fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value() noexcept
    : kind_(Kind::Null)
{
}

Value::
Value(
    Value const& other)
    : kind_(other.kind_)
{
    switch(kind_)
    {
    case Kind::Null:
        break;
    case Kind::Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Kind::Integer:
        std::construct_at(&i_, other.i_);
        break;
    case Kind::String:
        std::construct_at(&str_, other.str_);
        break;
    case Kind::Array:
        std::construct_at(&arr_, other.arr_);
        break;
    case Kind::Object:
        std::construct_at(&obj_, other.obj_);
        break;
    case Kind::Function:
        std::construct_at(&fn_, other.fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value(
    Value&& other) noexcept
    : kind_(other.kind_)
{
    switch(kind_)
    {
    case Kind::Null:
        break;
    case Kind::Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Kind::Integer:
        std::construct_at(&i_, other.i_);
        break;
    case Kind::String:
        std::construct_at(&str_, std::move(other.str_));
        std::destroy_at(&other.str_);
        break;
    case Kind::Array:
        std::construct_at(&arr_, std::move(other.arr_));
        std::destroy_at(&other.arr_);
        break;
    case Kind::Object:
        std::construct_at(&obj_, std::move(other.obj_));
        std::destroy_at(&other.obj_);
        break;
    case Kind::Function:
        std::construct_at(&fn_, std::move(other.fn_));
        std::destroy_at(&other.fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
    other.kind_ = Kind::Null;
}

Value&
Value::
operator=(
    Value&& other) noexcept
{
    Value temp(std::move(other));
    swap(temp);
    return *this;
}

Value&
Value::
operator=(
    Value const& other)
{
    Value temp(other);
    swap(temp);
    return *this;
}

//------------------------------------------------

Value::
Value(
    std::nullptr_t) noexcept
    : kind_(Kind::Null)
{
}

Value::
Value(
    std::int64_t i) noexcept
    : kind_(Kind::Integer)
    , i_(i)
{
}

Value::
Value(
    String str) noexcept
    : kind_(Kind::String)
    , str_(std::move(str))
{
}

Value::
Value(
    Array arr) noexcept
    : kind_(Kind::Array)
    , arr_(std::move(arr))
{
}

Value::
Value(
    Object obj) noexcept
    : kind_(Kind::Object)
    , obj_(std::move(obj))
{
}

Value::
Value(
    Function fn) noexcept
    : kind_(Kind::Function)
    , fn_(std::move(fn))
{
}

//------------------------------------------------

char const*
Value::
type_key() const noexcept
{
    switch(kind_)
    {
    case Kind::Null:        return "null";
    case Kind::Boolean:     return "bool";
    case Kind::Integer:     return "integer";
    case Kind::String:      return "string";
    case Kind::Array:       return arr_.type_key();
    case Kind::Object:      return obj_.type_key();
    case Kind::Function:    return fn_.type_key();
    default:
        MRDOX_UNREACHABLE();
    }
}

dom::Kind
Value::
kind() const noexcept
{
    switch(kind_)
    {
    case Kind::Null:        return dom::Kind::Null;
    case Kind::Boolean:     return dom::Kind::Boolean;
    case Kind::Integer:     return dom::Kind::Integer;
    case Kind::String:      return dom::Kind::String;
    case Kind::Array:       return dom::Kind::Array;
    case Kind::Object:      return dom::Kind::Object;
    case Kind::Function:    return dom::Kind::Function;
    default:
        MRDOX_UNREACHABLE();
    }
}

bool
Value::
isTruthy() const noexcept
{
    switch(kind_)
    {
    case Kind::Null:        return false;
    case Kind::Boolean:     return b_;
    case Kind::Integer:     return i_ != 0;
    case Kind::String:      return ! str_.empty();
    case Kind::Array:       return true;
    case Kind::Object:      return true;
    case Kind::Function:    return true;
    default:
        MRDOX_UNREACHABLE();
    }
}

Array const&
Value::
getArray() const
{
    if(kind_ == Kind::Array)
        return arr_;
    Error("not an Array").Throw();
}

Object const&
Value::
getObject() const
{
    if(kind_ == Kind::Object)
        return obj_;
    Error("not an Object").Throw();
}

Function const&
Value::
getFunction() const
{
    if(kind_ == Kind::Function)
        return fn_;
    Error("not a function").Throw();
}

void
Value::
swap(
    Value& other) noexcept
{
    Value temp(std::move(*this));
    std::destroy_at(this);
    std::construct_at(this, std::move(other));
    std::destroy_at(&other);
    std::construct_at(&other, std::move(temp));
}

//------------------------------------------------

void
JSON_escape(
    std::string& dest,
    std::string_view value)
{
    dest.reserve(dest.size() + value.size());
    for(auto c : value)
    {
        switch (c)
        {
        case '"':
            dest.append("\\\"");
            break;
        case '\\':
            dest.append("\\\\");
            break;
        case '\b':
            dest.append("\\b");
            break;
        case '\f':
            dest.append("\\f");
            break;
        case '\n':
            dest.append("\\n");
            break;
        case '\r':
            dest.append("\\r");
            break;
        case '\t':
            dest.append("\\t");
            break;
        default:
            dest.push_back(c);
            break;
        }
    }
}

static
void
JSON_stringify(
    std::string& dest,
    Value const& value,
    std::string& indent,
    std::unordered_set<void const*>& visited)
{
    switch(value.kind())
    {
    case Kind::Null:
        dest.append("null");
        break;
    case Kind::Boolean:
        if(value.getBool())
            dest.append("true");
        else
            dest.append("false");
        break;
    case Kind::Integer:
        dest.append(std::to_string(value.getInteger()));
        break;
    case Kind::String:
    {
        std::string_view const s =
            value.getString().get();
        dest.reserve(dest.size() +
            1 + s.size() + 1);
        dest.push_back('"');
        JSON_escape(dest, s);
        dest.push_back('"');
        break;
    }
    case Kind::Array:
    {
        auto const& arr = value.getArray();
        if(arr.empty())
        {
            dest.append("[]");
            break;
        }
        if(! visited.emplace(arr.impl().get()).second)
        {
            dest.append("\"[recursive]\"");
            break;
        }
        indent.append("    ");
        dest.append("[\n");
        for(std::size_t i = 0; i < arr.size(); ++i)
        {
            dest.append(indent);
            JSON_stringify(dest, arr.get(i), indent, visited);
            if(i != arr.size() - 1)
                dest.push_back(',');
            dest.push_back('\n');
        }
        indent.resize(indent.size() - 4);
        dest.append(indent);
        dest.append("]");
        break;
    }
    case Kind::Object:
    {
        auto const& obj = value.getObject();
        if(obj.empty())
        {
            dest.append("{}");
            break;
        }
        if(! visited.emplace(obj.impl().get()).second)
        {
            dest.append("\"{recursive}\"");
            break;
        }
        indent.append("    ");
        dest.append("{\n");
        for(auto it = obj.begin(); it != obj.end();)
        {
            auto it0 = it++;
            dest.append(indent);
            dest.push_back('"');
            JSON_escape(dest, it0->key);
            dest.append("\" : ");
            JSON_stringify(dest,
                it0->value, indent, visited);
            if(it != obj.end())
            {
                dest.push_back(',');
            }
            dest.push_back('\n');
        }
        indent.resize(indent.size() - 4);
        dest.append(indent);
        dest.append("}");
        break;
    }
    case Kind::Function:
        dest.append("\"(function)\"");
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string
JSON_stringify(
    Value const& value)
{
    std::string dest;
    std::string indent;
    std::unordered_set<void const*> visited;
    JSON_stringify(dest, value, indent, visited);
    return dest;
}

std::string
toString(
    Value const& value)
{
    switch(value.kind_)
    {
    case Value::Kind::Array:
        return toString(value.arr_);
    case Value::Kind::Object:
        return toString(value.obj_);
    default:
        return toStringChild(value);
    }
}

std::string
toStringChild(
    Value const& value)
{
    switch(value.kind_)
    {
    case Value::Kind::Null:
        return "null";
    case Value::Kind::Boolean:
        return value.b_ ? "true" : "false";
    case Value::Kind::Integer:
        return std::to_string(value.i_);
    case Value::Kind::String:
        return fmt::format("\"{}\"", value.str_);
    case Value::Kind::Array:
    {
        if(! value.arr_.empty())
            return "[...]";
        return "[]";
    }
    case Value::Kind::Object:
    {
        if(! value.obj_.empty())
            return "{...}";
        return "{}";
    }
    case Value::Kind::Function:
        return "[function]";
    default:
        MRDOX_UNREACHABLE();
    }
}

} // dom
} // mrdox
} // clang
