//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Function.hpp>

namespace clang {
namespace mrdox {
namespace dom {

namespace {

struct NullFunction : public FunctionImpl
{
    Expected<Value>
    call(Array const&) const override
    {
        return dom::Value(dom::Kind::Undefined);
    }
};

static Function const nullFunction =
    newFunction<NullFunction>();

} // (anon)

Function::
~Function() = default;

Function::
Function() noexcept
    : Function(nullFunction)
{
}

Function::
Function(
    Function&& other) noexcept
    : Function()
{
    swap(other);
}

Function::
Function(
    Function const& other) noexcept
    : impl_(other.impl_)
{
}

Function&
Function::
operator=(
    Function&& other) noexcept
{
    Function temp(std::move(other));
    swap(temp);
    return *this;
}

Function&
Function::
operator=(
    Function const& other) noexcept
{
    if(this != &other)
        impl_ = other.impl_;
    return *this;
}

//------------------------------------------------
//
// FunctionImpl
//
//------------------------------------------------

char const*
FunctionImpl::
type_key() const noexcept
{
    return "function";
}

} // dom
} // mrdox
} // clang
