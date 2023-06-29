//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Dom.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/RangeFor.hpp>
#include <algorithm>

namespace clang {
namespace mrdox {
namespace dom {

//------------------------------------------------
//
// Array
//
//------------------------------------------------

namespace {

class DefaultArrayImpl : public ArrayImpl
{
    std::vector<Value> v_;

public:
    std::size_t
    size() const noexcept override
    {
        return v_.size();
    }

    Value
    at(std::size_t i) const override
    {
        if(i < v_.size())
            return v_[i];
        throw std::out_of_range("v_[i]");
    }
};

} // (anon)

ArrayImpl::
~ArrayImpl() = default;

Array::
~Array() = default;

Array::
Array()
    : impl_(std::make_shared<DefaultArrayImpl>())
{
}

Array::
Array(
    Array const& other)
    : impl_(other.impl_)
{
}

std::string
toString(
    Array const& arr)
{
    if(arr.empty())
        return "[]";
    std::string s = "[";
    {
        auto const n = arr.size();
        auto insert = std::back_inserter(s);
        for(std::size_t i = 0; i < n; ++i)
        {
            if(i != 0)
                fmt::format_to(insert,
                    ", {}",
                    toString(arr.at(i)));
            else
                fmt::format_to(insert,
                    " {}",
                    toStringChild(arr.at(i)));
        }
    }
    s += " ]";
    return s;
}

//------------------------------------------------
//
// Object
//
//------------------------------------------------

ObjectImpl::
~ObjectImpl() = default;

Object::
~Object() = default;

Object::
Object()
    : impl_(std::make_shared<DefaultObjectImpl>())
{
}

Object::
Object(
    Object const& other) noexcept
    : impl_(other.impl_)
{
}

Object::
Object(
    entries_type list)
    : impl_(std::make_shared<
        DefaultObjectImpl>(std::move(list)))
{
}

std::string
toString(
    Object const& obj)
{
    if(obj.empty())
        return "{}";
    std::string s = "{";
    {
        auto insert = std::back_inserter(s);
        for(auto const& kv : RangeFor(obj.entries()))
        {
            if(! kv.first)
                s.push_back(',');
            fmt::format_to(insert,
                " {} : {}",
                kv.value.first,
                toStringChild(kv.value.second));
        }
    }
    s += " }";
    return s;
}

//------------------------------------------------
//
// DefaultObjectImpl
//
//------------------------------------------------

DefaultObjectImpl::
DefaultObjectImpl() noexcept = default;

DefaultObjectImpl::
DefaultObjectImpl(
    entries_type entries) noexcept
    : entries_(std::move(entries))
{
}

std::size_t
DefaultObjectImpl::
size() const noexcept
{
    return entries_.size();
}

bool
DefaultObjectImpl::
exists(
    std::string_view key) const
{
    return std::find_if(
        entries_.begin(), entries_.end(),
        [key](value_type const& kv)
        {
            return kv.first == key;
        }) != entries_.end();
}

Value
DefaultObjectImpl::
get(std::string_view key) const
{
    auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [key](value_type const& kv)
        {
            return kv.first == key;
        });
    if(it != entries_.end())
        return it->second;
    return nullptr;
}

void
DefaultObjectImpl::
set(std::string_view key, Value value)
{
    auto it =  std::find_if(
        entries_.begin(), entries_.end(),
        [key](value_type const& kv)
        {
            return kv.first == key;
        });
    if(it == entries_.end())
        entries_.emplace_back(
            key, std::move(value));
    else
        it->second = std::move(value);
}

auto
DefaultObjectImpl::
entries() const ->
    entries_type
{
    return entries_;
}

//------------------------------------------------
//
// LazyObjectImpl
//
//------------------------------------------------

ObjectImpl&
LazyObjectImpl::
obj() const
{
    auto impl = sp_.load();
    if(impl)
        return *impl;
    impl_type expected = nullptr;
    if(sp_.compare_exchange_strong(
            expected, construct().impl()))
        return *sp_.load();
    return *expected;
}

std::size_t
LazyObjectImpl::
size() const noexcept
{
    return obj().size();
}

bool
LazyObjectImpl::
exists(
    std::string_view key) const
{
    return obj().exists(key);
}

Value
LazyObjectImpl::
get(std::string_view key) const
{
    return obj().get(key);
}

void
LazyObjectImpl::
set(
    std::string_view key, Value value)
{
    obj().set(key, value);
}

auto
LazyObjectImpl::
entries() const ->
    entries_type
{
    return obj().entries();
}

//------------------------------------------------
//
// Value
//
//------------------------------------------------

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
    default:
        MRDOX_UNREACHABLE();
    }
    other.kind_ = Kind::Null;
}

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
    std::string s) noexcept
    : kind_(Kind::String)
    , str_(std::move(s))
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
    case Kind::Null: return false;
    case Kind::Boolean: return b_;
    case Kind::Integer: return i_ != 0;
    case Kind::String: return str_.size() > 0;
    case Kind::Array: return ! arr_.empty();
    case Kind::Object:
        // this can call construct()
        return ! obj_.empty();
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
    throw Error("not an Array");
}

Object const&
Value::
getObject() const
{
    if(kind_ == Kind::Object)
        return obj_;
    throw Error("not an Object");
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
    default:
        MRDOX_UNREACHABLE();
    }
}

//------------------------------------------------

} // dom
} // mrdox
} // clang
