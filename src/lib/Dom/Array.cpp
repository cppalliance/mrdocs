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
// ArrayImpl
//
//------------------------------------------------

ArrayImpl::
~ArrayImpl() = default;

char const*
ArrayImpl::
type_key() const noexcept
{
    return "array";
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
    MRDOX_ASSERT(i < elements_.size());
    return elements_[i];
}

void
DefaultArrayImpl::
emplace_back(
    value_type value)
{
    elements_.emplace_back(std::move(value));
}

} // dom
} // mrdox
} // clang
