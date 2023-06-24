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

namespace clang {
namespace mrdox {
namespace dom {

Any::
Any() noexcept = default;

Any::
Any(Any const&) noexcept
{
    MRDOX_ASSERT(refs_ == 1);
}

Any::
~Any() = default;

//------------------------------------------------

std::size_t
Array::
length() const noexcept
{
    return 0;
}

Value
Array::
get(std::size_t) const
{
    return nullptr;
}

//------------------------------------------------

Object::
Object(
    Object const&) = default;

Object::
Object() noexcept = default;

Object::
Object(
    list_type list)
    : list_(std::move(list))
{
}

auto
Object::
list() const noexcept ->
    list_type const&
{
    return list_;
}

void
Object::
append(
    std::string_view key,
    Value value)
{
    list_.emplace_back(key, value);
}

void
Object::
append(
    std::initializer_list<value_type> init)
{
    list_.insert(list_.end(), init);
}

Value
Object::
copy() const
{
    return create<Object>(list_);
}

bool
Object::
empty() const noexcept
{
    return list_.empty();
}

Value
Object::
get(std::string_view key) const
{
    auto it = std::find_if(
        list_.begin(), list_.end(),
        [key](value_type const& kv)
        {
            return kv.first == key;
        });
    if(it != list_.end())
        return it->second;
    return nullptr;
}

void
Object::
set(std::string_view key,
    Value value)
{
    auto it = std::find_if(
        list_.begin(), list_.end(),
        [key](value_type& kv)
        {
            return kv.first == key;
        });
    if(it != list_.end())
    {
        if(it != list_.end() - 1)
            *it = list_.back();
        list_.resize(list_.size() - 1);
    }
    list_.emplace_back(
        key, std::move(value));
}

auto
Object::
props() const ->
    std::vector<std::string_view>
{
    std::vector<std::string_view> list;
    list.reserve(list_.size());
    for(auto const& kv : list_)
        list.emplace_back(kv.first);
    return list;
}

ObjectPtr
createObject(
    Object::list_type list)
{
    return create<Object>(std::move(list));
}

//------------------------------------------------

LazyObject::
LazyObject() noexcept = default;

LazyObject::
~LazyObject() noexcept
{
    if(auto obj = p_.load())
        obj->release();
}

ObjectPtr
LazyObject::
get() const
{
    if(Object* obj = p_.load())
        return obj;
    // If there is a data race, there might
    // be one or more superfluous constructions.
    ObjectPtr obj = construct();
    Object* expected = nullptr;
    if(p_.compare_exchange_strong(
        expected, obj.get()))
    {
        obj->addref();
        return obj;
    }
    return expected;
}

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
        str_.~basic_string();
        break;
    case Kind::Array:
        arr_.~Pointer();
        break;
    case Kind::Object:
        obj_.~Pointer();
        break;
    case Kind::LazyObject:
        lazy_obj_.~Pointer();
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
        b_ = other.b_;
        break;
    case Kind::Integer:
        i_ = other.i_;
        break;
    case Kind::String:
        new(&str_) std::string(other.str_);
        break;
    case Kind::Array:
        new(&arr_) ArrayPtr(other.arr_);
        break;
    case Kind::Object:
        new(&obj_) ObjectPtr(other.obj_);
        break;
    case Kind::LazyObject:
        new(&lazy_obj_) LazyObjectPtr(other.lazy_obj_);
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
        b_ = other.b_;
        break;
    case Kind::Integer:
        i_ = other.i_;
        break;
    case Kind::String:
        new(&str_) std::string(std::move(other.str_));
        other.str_.~basic_string();
        break;
    case Kind::Array:
        new(&arr_) ArrayPtr(std::move(other.arr_));
        other.arr_.~Pointer();
        break;
    case Kind::Object:
        new(&obj_) ObjectPtr(std::move(other.obj_));
        other.obj_.~Pointer();
        break;
    case Kind::LazyObject:
        new(&lazy_obj_) LazyObjectPtr(std::move(other.lazy_obj_));
        other.lazy_obj_.~Pointer();
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
    ArrayPtr arr) noexcept
    : kind_(Kind::Array)
    , arr_(std::move(arr))
{
}

Value::
Value(
    ObjectPtr obj) noexcept
    : kind_(Kind::Object)
    , obj_(std::move(obj))
{
}

Value::
Value(
    LazyObjectPtr lazy_obj) noexcept
    : kind_(Kind::LazyObject)
    , lazy_obj_(std::move(lazy_obj))
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

Value
Value::
copy() const
{
    switch(kind_)
    {
    case Kind::Null:
    case Kind::Boolean:
    case Kind::Integer:
    case Kind::String:
        return *this;
    case Kind::Array:
        // VFALCO currently, arrays are immutable so
        // we can just give the caller shared ownership.
        return *this;
    case Kind::Object:
    case Kind::LazyObject:
        return obj_->copy();
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
    case Kind::Object:
    case Kind::LazyObject:  return dom::Kind::Object;
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
    case Kind::Array: return arr_->length() > 0;
    case Kind::Object: return ! obj_->empty();
    case Kind::LazyObject: return ! lazy_obj_->get()->empty();
    default: MRDOX_UNREACHABLE();
    }
}

ObjectPtr
Value::
getObject() const noexcept
{
    if(kind_ == Kind::Object)
        return obj_;
    if(kind_ == Kind::LazyObject)
        return lazy_obj_->get();
    MRDOX_ASSERT(kind_ == Kind::Object);
    return obj_;
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

} // dom
} // mrdox
} // clang
