//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Support/RangeFor.hpp>
#include <atomic>
#include <format>
#include <ranges>

namespace clang {
namespace mrdocs {
namespace dom {

//------------------------------------------------
//
// Object
//
//------------------------------------------------

Object::
Object()
    : impl_(std::make_shared<DefaultObjectImpl>())
{
}

Object::
Object(
    Object&& other)
    : Object()
{
    swap(other);
}

Object::
Object(
    Object const& other) noexcept
    : impl_(other.impl_)
{
}

Object&
Object::
operator=(Object&& other)
{
    Object temp;
    swap(temp);
    swap(other);
    return *this;
}

Object&
Object::
operator=(Object const& other) noexcept
{
    Object temp(other);
    swap(temp);
    return *this;
}

//------------------------------------------------

Object::
Object(
    storage_type list)
    : impl_(std::make_shared<
        DefaultObjectImpl>(std::move(list)))
{
}

bool
Object::
exists(std::string_view key) const
{
    return impl_->exists(key);
}

bool
operator==(Object const& a, Object const& b) noexcept
{
    return a.impl_ == b.impl_;
}

std::string
toString(Object const& obj)
{
  return std::format("[object {}]", obj.type_key());
}

//------------------------------------------------
//
// ObjectImpl
//
//------------------------------------------------

ObjectImpl::
~ObjectImpl() = default;

char const*
ObjectImpl::
type_key() const noexcept
{
    return "Object";
}

bool
ObjectImpl::exists(std::string_view key) const
{
    return !visit([&](String const& k, Value const&)
    {
        if (k == key)
        {
            return false;
        }
        return true;
    });
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
    storage_type entries) noexcept
    : entries_(std::move(entries))
{
}

std::size_t
DefaultObjectImpl::
size() const
{
    return entries_.size();
}

auto
DefaultObjectImpl::
get(std::string_view key) const ->
    Value
{
    auto it = std::ranges::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
    {
        return kv.key == key;
    });
    if (it == entries_.end())
    {
        return Kind::Undefined;
    }
    return it->value;
}

void
DefaultObjectImpl::
set(String key, Value value)
{

    auto it = std::ranges::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
        {
            return kv.key == key;
        });
    if(it == entries_.end())
        entries_.emplace_back(
            key, std::move(value));
    else
        it->value = std::move(value);
}

bool
DefaultObjectImpl::
visit(std::function<bool(String, Value)> visitor) const
{
    for (auto const& kv : entries_)
    {
        if (!visitor(kv.key, kv.value))
        {
            return false;
        }
    }
    return true;
}

bool
DefaultObjectImpl::exists(std::string_view key) const {
    auto it = std::ranges::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
        {
            return kv.key == key;
        });
    return it != entries_.end();
}

} // dom
} // mrdocs
} // clang
