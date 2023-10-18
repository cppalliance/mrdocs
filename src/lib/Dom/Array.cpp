//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Array.hpp>

namespace clang {
namespace mrdox {
namespace dom {

void
ArrayImpl::
set(size_type, Value) {}

Array::
~Array() = default;

Array::
Array()
    : impl_(std::make_shared<DefaultArrayImpl>())
{
}

Array::
Array(
    Array&& other)
    : Array()
{
    swap(other);
}

Array::
Array(
    Array const& other)
    : impl_(other.impl_)
{
}

Array::
Array(storage_type elements)
    : impl_(std::make_shared<DefaultArrayImpl>())
{
    for(auto const& e : elements)
        impl_->emplace_back(e);
}

Array&
Array::
operator=(
    Array&& other)
{
    Array temp;
    swap(temp);
    swap(other);
    return *this;
}

bool
operator==(dom::Array const& a, dom::Array const& b) noexcept
{
    if (a.impl_ == b.impl_)
    {
        return true;
    }
    Array::size_type const an = a.size();
    Array::size_type const bn = b.size();
    if (an != bn)
    {
        return false;
    }
    for (std::size_t i = 0; i < an; ++i)
    {
        if (a.get(i) != b.get(i))
        {
            return false;
        }
    }
    return true;
}

std::strong_ordering
operator<=>(Array const& a, Array const& b) noexcept
{
    Array::size_type const an = a.size();
    Array::size_type const bn = b.size();
    if (an < bn)
    {
        return std::strong_ordering::less;
    }
    if (bn < an)
    {
        return std::strong_ordering::less;
    }
    Array::size_type const n = an;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (a.get(i) < b.get(i))
        {
            return std::strong_ordering::less;
        }
        if (b.get(i) < a.get(i))
        {
            return std::strong_ordering::greater;
        }
    }
    return std::strong_ordering::equal;
}

std::string
toString(
    Array const& arr)
{
    if(arr.empty())
        return "";
    std::string s;
    auto const n = arr.size();
    if (n != 0) {
        s += toString(arr.at(0));
        for(std::size_t i = 1; i < n; ++i)
        {
            s += ',';
            s += toString(arr.at(i));
        }
    }
    return s;
}

//------------------------------------------------
//
// ArrayImpl
//
//------------------------------------------------

ArrayImpl::
~ArrayImpl() = default;

char const*
ArrayImpl::
type_key() const noexcept
{
    return "Array";
}

void
ArrayImpl::
emplace_back(
    value_type value)
{
    Error("Array is const").Throw();
}

//------------------------------------------------
//
// DefaultArrayImpl
//
//------------------------------------------------

DefaultArrayImpl::
DefaultArrayImpl() = default;

DefaultArrayImpl::
DefaultArrayImpl(
    storage_type elements) noexcept
    : elements_(std::move(elements))
{
}

auto
DefaultArrayImpl::
size() const ->
    size_type
{
    return elements_.size();
}

auto
DefaultArrayImpl::
get(
    size_type i) const ->
        value_type
{
    if (i < elements_.size())
    {
        return elements_[i];
    }
    return {};
}

void
DefaultArrayImpl::
set(size_type i, Value v)
{
    if (i >= elements_.size())
    {
        elements_.resize(i + 1, Kind::Undefined);
    }
    elements_[i] = std::move(v);
}

void
DefaultArrayImpl::
emplace_back(
    value_type value)
{
    elements_.emplace_back(std::move(value));
}

char const*
DefaultArrayImpl::
type_key() const noexcept
{
    return "Array";
}

} // dom
} // mrdox
} // clang
