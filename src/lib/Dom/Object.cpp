//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Object.hpp>
#include <mrdox/Support/RangeFor.hpp>
#include <fmt/format.h>
#include <atomic>
#include <ranges>

namespace clang {
namespace mrdox {
namespace dom {

static_assert(std::random_access_iterator<Object::iterator>);
static_assert(std::ranges::random_access_range<Object>);

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
    for(auto const& kv : *this)
        if(kv.key == key)
            return true;

    return false;
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
        for(auto const& kv : RangeFor(obj))
        {
            if(! kv.first)
                s.push_back(',');
            fmt::format_to(insert,
                " {} : {}",
                kv.value.key,
                toStringChild(kv.value.value));
        }
    }
    s += " }";
    return s;
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
    return "object";
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
get(std::size_t i) const ->
    reference
{
    MRDOX_ASSERT(i < entries_.size());
    return entries_[i];
}

Value
DefaultObjectImpl::
find(std::string_view key) const
{
    auto it = std::ranges::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
        {
            return kv.key == key;
        });
    if(it == entries_.end())
        return nullptr;
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
size() const
{
    return obj().size();
}

auto
LazyObjectImpl::
get(std::size_t i) const ->
    reference
{
    return obj().get(i);
}

Value
LazyObjectImpl::
find(std::string_view key) const
{
    return obj().find(key);
}

void
LazyObjectImpl::
set(String key, Value value)
{
    obj().set(std::move(key), value);
}

} // dom
} // mrdox
} // clang
