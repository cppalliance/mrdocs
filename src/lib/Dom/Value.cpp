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
#include <algorithm>

namespace clang {
namespace mrdox {
namespace dom {

Value::
~Value()
{
    switch(kind_)
    {
    using enum Kind;
    case Undefined:
    case Null:
    case Boolean:
    case Integer:
        break;
    case String:
    case SafeString:
        std::destroy_at(&str_);
        break;
    case Array:
        std::destroy_at(&arr_);
        break;
    case Object:
        std::destroy_at(&obj_);
        break;
    case Function:
        std::destroy_at(&fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value() noexcept
    : kind_(Kind::Undefined)
{
}

Value::
Value(
    Value const& other)
    : kind_(other.kind_)
{
    switch(kind_)
    {
    using enum Kind;
    case Undefined:
    case Null:
        break;
    case Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Integer:
        std::construct_at(&i_, other.i_);
        break;
    case String:
    case SafeString:
        std::construct_at(&str_, other.str_);
        break;
    case Array:
        std::construct_at(&arr_, other.arr_);
        break;
    case Object:
        std::construct_at(&obj_, other.obj_);
        break;
    case Function:
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
    using enum clang::mrdox::dom::Kind;
    switch(kind_)
    {
    case Undefined:
    case Null:
        break;
    case Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Integer:
        std::construct_at(&i_, other.i_);
        break;
    case String:
    case SafeString:
        std::construct_at(&str_, std::move(other.str_));
        std::destroy_at(&other.str_);
        break;
    case Array:
        std::construct_at(&arr_, std::move(other.arr_));
        std::destroy_at(&other.arr_);
        break;
    case Object:
        std::construct_at(&obj_, std::move(other.obj_));
        std::destroy_at(&other.obj_);
        break;
    case Function:
        std::construct_at(&fn_, std::move(other.fn_));
        std::destroy_at(&other.fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
    other.kind_ = Null;
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
    if (this == &other)
    {
        return *this;
    }
    Value temp(other);
    swap(temp);
    return *this;
}

//------------------------------------------------

Value::
Value(dom::Kind kind) noexcept
    : kind_(kind)
{
    switch(kind_)
    {
    using enum Kind;
    case Undefined:
    case Null:
        break;
    case Boolean:
        std::construct_at(&b_, false);
        break;
    case Integer:
        std::construct_at(&i_, 0);
        break;
    case String:
    case SafeString:
        std::construct_at(&str_);
        break;
    case Array:
        std::construct_at(&arr_);
        break;
    case Object:
        std::construct_at(&obj_);
        break;
    case Function:
        std::construct_at(&fn_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value(
    std::nullptr_t) noexcept
    : kind_(Kind::Null)
{
}

Value::
Value(
    std::int64_t v) noexcept
    : kind_(Kind::Integer)
    , i_(v)
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
    using enum Kind;
    case Undefined:   return "undefined";
    case Null:        return "null";
    case Boolean:     return "boolean";
    case Integer:     return "integer";
    case String:      return "string";
    case SafeString:  return "safeString";
    case Array:       return arr_.type_key();
    case Object:      return obj_.type_key();
    case Function:    return fn_.type_key();
    default:
        MRDOX_UNREACHABLE();
    }
}

dom::Kind
Value::
kind() const noexcept
{
    return kind_;
}

bool
Value::
isTruthy() const noexcept
{
    switch(kind_)
    {
    using enum Kind;
    case Boolean:
        return b_;
    case Integer:
        return i_ != 0;
    case SafeString:
    case String:
        return !str_.empty();
    case Array:
    case Object:
    case Function:
        return true;
    case Null:
    case Undefined:
        return false;
    default:
        MRDOX_UNREACHABLE();
    }
}

Array const&
Value::
getArray() const
{
    if(kind_ == Kind::Array)
    {
        return arr_;
    }
    Error("not an Array").Throw();
}

Array&
Value::
getArray()
{
    if(kind_ == Kind::Array)
    {
        return arr_;
    }
    Error("not an Array").Throw();
}

Object const&
Value::
getObject() const
{
    if(kind_ == Kind::Object)
    {
        return obj_;
    }
    Error("not an Object").Throw();
}

Function const&
Value::
getFunction() const
{
    if(kind_ == Kind::Function)
    {
        return fn_;
    }
    Error("not a function").Throw();
}

dom::Value
Value::
get(std::string_view key) const
{
    if (kind_ == Kind::Object)
    {
        return obj_.get(key);
    }
    if (kind_ == Kind::Array || kind_ == Kind::String)
    {
        auto isDigit = [](auto c) {
            return c >= '0' && c <= '9';
        };
        if (std::ranges::all_of(key, isDigit))
        {
            std::size_t idx = 0;
            auto res = std::from_chars(
                key.data(), key.data() + key.size(), idx);
            if (res.ec == std::errc())
            {
                if (kind_ == Kind::String && idx < str_.size())
                {
                    return String(std::string_view(
                        str_.get().data() + idx, 1));
                }
                else if (kind_ == Kind::Array && idx < arr_.size())
                {
                    return arr_.get(idx);
                }
            }
        }
    }
    return {};
}

dom::Value
Value::
get(std::size_t i) const
{
    if (kind_ == Kind::Array)
    {
        return arr_.get(i);
    }
    if (kind_ == Kind::String)
    {
        if (i < str_.size())
        {
            return str_.get()[i];
        }
    }
    if (kind_ == Kind::Object)
    {
        std::string key = std::to_string(i);
        return obj_.get(key);
    }
    return {};
}

dom::Value
Value::
get(dom::Value const& i) const
{
    if (i.isInteger())
    {
        return get(static_cast<std::size_t>(i.getInteger()));
    }
    if (i.isString() || i.isSafeString())
    {
        return get(i.getString().get());
    }
    return {};
}

dom::Value
Value::
lookup(std::string_view keys) const
{
    dom::Value cur = *this;
    std::size_t pos = keys.find('.');
    std::string_view key = keys.substr(0, pos);
    while (pos != std::string_view::npos)
    {
        cur = cur.get(key);
        if (cur.isUndefined())
        {
            return cur;
        }
        keys = keys.substr(pos + 1);
        pos = keys.find('.');
        key = keys.substr(0, pos);
    }
    return cur.get(key);
}

void
Value::
set(String const& key, Value const& value)
{
    if (isObject())
    {
        obj_.set(key, value);
        return;
    }
    if (isArray())
    {
        std::string_view idxStr = key;
        std::size_t idx = 0;
        auto res = std::from_chars(
            idxStr.data(),
            idxStr.data() + idxStr.size(),
            idx);
        if (res.ec == std::errc())
        {
            arr_.set(idx, value);
        }
        return;
    }
}

bool
Value::
exists(std::string_view key) const
{
    if (kind_ == Kind::Object)
    {
        return obj_.exists(key);
    }
    if (kind_ == Kind::Array)
    {
        auto isDigit = [](auto c) {
            return c >= '0' && c <= '9';
        };
        if (std::ranges::all_of(key, isDigit))
        {
            return
                std::stoi(std::string(key)) <
                static_cast<int>(arr_.size());
        }
    }
    return false;
}

bool
Value::
empty() const
{
    switch(kind_)
    {
    using enum clang::mrdox::dom::Kind;
    case Undefined:
    case Null:
        return true;
    case Boolean:
    case Integer:
        return false;
    case String:
    case SafeString:
        return str_.empty();
    case Array:
        return arr_.empty();
    case Object:
        return obj_.empty();
    case Function:
        return false;
    default:
        MRDOX_UNREACHABLE();
    }
}

std::size_t
Value::
size() const
{
    switch(kind_)
    {
    using enum clang::mrdox::dom::Kind;
    case Undefined:
    case Null:
        return 0;
    case Boolean:
    case Integer:
        return 1;
    case String:
    case SafeString:
        return str_.size();
    case Array:
        return arr_.size();
    case Object:
        return obj_.size();
    case Function:
        return 1;
    default:
        MRDOX_UNREACHABLE();
    }
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

dom::Value
operator+(Value const& lhs, Value const& rhs)
{
    if (lhs.kind() == rhs.kind())
    {
        switch (lhs.kind())
        {
        using enum Kind;
        case Integer:
            return lhs.getInteger() + rhs.getInteger();
        case String:
            return lhs.getString() + rhs.getString();
        case SafeString:
        {
            dom::Value res = lhs.getString() + rhs.getString();
            res.kind_ = SafeString;
            return res;
        }
        case Undefined:
        case Null:
            return std::numeric_limits<std::int64_t>::quiet_NaN();
        case Boolean:
            return lhs;
        case Array:
            return lhs.getArray() + rhs.getArray();
        case Object:
        case Function:
            return {};
        default:
            MRDOX_UNREACHABLE();
        }
    }
    bool const lhsIsArithmetic = lhs.isInteger() || lhs.isBoolean();
    bool const rhsIsArithmetic = rhs.isInteger() || rhs.isBoolean();
    if (lhsIsArithmetic && rhsIsArithmetic)
    {
        if (lhs.isBoolean() && rhs.isInteger())
        {
            return static_cast<std::int64_t>(lhs.getBool()) + rhs.getInteger();
        }
        if (lhs.isInteger() && rhs.isBoolean())
        {
            return lhs.getInteger() + static_cast<std::int64_t>(rhs.getBool());
        }
    }
    bool const lhsIsInvalidSum = lhs.isNull() || lhs.isUndefined();
    bool const rhsIsInvalidSum = rhs.isNull() || rhs.isUndefined();
    if (lhsIsInvalidSum && rhsIsInvalidSum)
    {
        return std::numeric_limits<std::int64_t>::quiet_NaN();
    }
    dom::Value res = toString(lhs) + toString(rhs);
    if (lhs.isSafeString())
    {
        res.kind_ = lhs.kind_;
    }
    return res;
}

dom::Value
operator||(Value const& lhs, Value const& rhs)
{
    if (lhs.isTruthy())
    {
        return lhs;
    }
    return rhs;
}

dom::Value
operator&&(Value const& lhs, Value const& rhs)
{
    if (!lhs.isTruthy())
    {
        return lhs;
    }
    return rhs;
}

//------------------------------------------------

namespace JSON
{
void
escape(
    std::string& dest,
    std::string_view value)
{
    dest.reserve(dest.size() + value.size());
    for(auto c : value)
    {
        switch (c)
        {
        case '"':
            dest.append(R"(\")");
            break;
        case '\\':
            dest.append(R"(\\)");
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
stringify(
    std::string& dest,
    Value const& value,
    std::string& indent,
    std::unordered_set<void const*>& visited)
{
    switch(value.kind())
    {
    case Kind::Undefined:
    case Kind::Null:
    case Kind::Function:
        dest.append("null");
        break;
    case Kind::Boolean:
        if(value.getBool())
        {
            dest.append("true");
        }
        else
        {
            dest.append("false");
        }
        break;
    case Kind::Integer:
        dest.append(std::to_string(value.getInteger()));
        break;
    case Kind::String:
    case Kind::SafeString:
    {
        std::string_view const s =
            value.getString().get();
        dest.reserve(dest.size() +
            1 + s.size() + 1);
        dest.push_back('"');
        JSON::escape(dest, s);
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
        Array::size_type n = arr.size();
        for(std::size_t i = 0; i < n; ++i)
        {
            dom::Value value = arr.get(i);
            if (value.isUndefined() || value.isFunction())
            {
                continue;
            }
            dest.append(indent);
            stringify(dest, value, indent, visited);
            if(i != n - 1)
            {
                dest.push_back(',');
            }
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
        bool is_first = true;
        obj.visit([&](String const& key, Value const& value)
        {
            if (value.isUndefined() || value.isFunction())
            {
                return;
            }
            if (!is_first)
            {
                dest.push_back(',');
                dest.push_back('\n');
            }
            is_first = false;
            dest.append(indent);
            dest.push_back('"');
            JSON::escape(dest, key);
            dest.append("\": ");
            stringify(dest, value, indent, visited);
        });
        dest.push_back('\n');
        indent.resize(indent.size() - 4);
        dest.append(indent);
        dest.append("}");
        break;
    }
    default:
        MRDOX_UNREACHABLE();
    }
}

std::string
stringify(
    Value const& value)
{
    std::string dest;
    std::string indent;
    std::unordered_set<void const*> visited;
    stringify(dest, value, indent, visited);
    return dest;
}
}

bool
operator==(
    Value const& lhs,
    Value const& rhs) noexcept
{
    if (lhs.kind_ != rhs.kind_)
    {
        return false;
    }
    switch(lhs.kind_)
    {
        using enum Kind;
        case Undefined:
        case Null:
            return true;
        case Boolean:
            return lhs.b_ == rhs.b_;
        case Integer:
            return lhs.i_ == rhs.i_;
        case String:
        case SafeString:
            return lhs.str_ == rhs.str_;
        case Array:
            return lhs.arr_ == rhs.arr_;
        case Object:
            return lhs.obj_ == rhs.obj_;
        case Function:
            return lhs.fn_.impl() == rhs.fn_.impl();
        default:
            MRDOX_UNREACHABLE();
    }
}

std::strong_ordering
operator<=>(dom::Value const& lhs, dom::Value const& rhs) noexcept
{
    using kind_t = std::underlying_type_t<dom::Kind>;
    if (static_cast<kind_t>(lhs.kind()) < static_cast<kind_t>(rhs.kind()))
    {
        return std::strong_ordering::less;
    }
    if (static_cast<kind_t>(rhs.kind()) < static_cast<kind_t>(lhs.kind()))
    {
        return std::strong_ordering::greater;
    }
    switch (lhs.kind())
    {
    using enum Kind;
    case Undefined:
    case Null:
        return std::strong_ordering::equivalent;
    case Boolean:
        return lhs.b_ <=> rhs.b_;
    case Integer:
        return lhs.i_ <=> rhs.i_;
    case String:
    case SafeString:
        return lhs.str_ <=> rhs.str_;
    case Array:
        return lhs.arr_ <=> rhs.arr_;
    case Object:
        return lhs.obj_ <=> rhs.obj_;
    case Function:
        if (lhs.fn_.impl() == rhs.fn_.impl())
        {
            return std::strong_ordering::equal;
        }
        return std::strong_ordering::equivalent;
    default:
        MRDOX_UNREACHABLE();
    }
    return std::strong_ordering::greater;
}

std::string
toString(
    Value const& value)
{
    switch(value.kind_)
    {
    case Kind::Array:
        return toString(value.arr_);
    case Kind::Object:
        return toString(value.obj_);
    case Kind::Undefined:
        return "undefined";
    case Kind::Null:
        return "null";
    case Kind::Boolean:
        return value.b_ ? "true" : "false";
    case Kind::Integer:
        return std::to_string(value.i_);
    case Kind::String:
    case Kind::SafeString:
        return std::string(value.str_.get());
    case Kind::Function:
        return "[object Function]";
    default:
        MRDOX_UNREACHABLE();
    }
}

} // dom
} // mrdox
} // clang
