//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Handlebars.hpp>
#include "duktape.h"
#include <llvm/Support/MemoryBuffer.h>

namespace clang {
namespace mrdox {
namespace hbs {

//------------------------------------------------

// The current duk_context
thread_local static ::duk_context* ctx_ = nullptr;

//------------------------------------------------

struct Context::Impl
{
    std::size_t refs;
    ::duk_context* ctx;

    ~Impl()
    {
        ::duk_destroy_heap(ctx_);
    }

    Impl()
        : refs(1)
        , ctx(::duk_create_heap_default())
    {
    }
};

Context::
~Context()
{
    if(--impl_->refs == 0)
        delete impl_;
}

Context::
Context()
    : impl_(new Impl)
{
}

Context::
Context(
    Context const& other) noexcept
    : impl_(other.impl_)
{
    ++impl_->refs;
}

std::error_code
Context::
eval(
    std::string_view js)
{
    // VFALCO How do we handle the error if any?
    duk_eval_lstring_noresult(impl_->ctx, js.data(), js.size());
    return {};
}

std::error_code
Context::
eval_file(
    std::string_view path)
{
    auto js = llvm::MemoryBuffer::getFile(path);
    if(! js)
        return js.getError();
    return eval(js->get()->getBuffer());
}

//------------------------------------------------

struct Access
{
    static int idx(Array const& arr) noexcept
    {
        return arr.idx_;
    }

    static int idx(Object const& obj) noexcept
    {
        return obj.idx_;
    }

    static ::duk_context* ctx(Context const& ctx) noexcept
    {
        return ctx.impl_->ctx;
    }
};

//------------------------------------------------

Array::
~Array()
{
    if(idx_ != DUK_INVALID_INDEX)
    {
        // remove idx_ from stack?
    }
}

Array::
Array(
    Context const& ctx)
    : ctx_(ctx)
    , idx_(::duk_push_array(Access::ctx(ctx)))
{
}

void
Array::
append(
    std::string_view value) const
{
}

void
Array::
append(
    Array const& value) const
{
}

void
Array::
append(
    Object const& value) const
{
}

//------------------------------------------------

Object::
~Object()
{
    if(idx_ != DUK_INVALID_INDEX)
    {
        // remove idx_ from stack?
    }
}

Object::
Object(
    Context const& ctx)
    : ctx_(ctx)
    , idx_(::duk_push_object(Access::ctx(ctx)))
{
}

void
Object::
insert(
    std::string_view key,
    std::string_view value) const
{
    ::duk_push_lstring(Access::ctx(ctx_), value.data(), value.size());
    ::duk_put_prop_lstring(Access::ctx(ctx_), idx_, key.data(), key.size());
}

void
Object::
insert(
    std::string_view key,
    Array const& value) const
{
    ::duk_dup(Access::ctx(ctx_), Access::idx(value));
    ::duk_put_prop_lstring(Access::ctx(ctx_), idx_, key.data(), key.size());
}

void
Object::
insert(
    std::string_view key,
    Object const& value) const
{
    ::duk_dup(Access::ctx(ctx_), Access::idx(value));
    ::duk_put_prop_lstring(Access::ctx(ctx_), idx_, key.data(), key.size());
}

//------------------------------------------------

Handlebars::
Handlebars(
    Context const& ctx) noexcept
    : ctx_(ctx)
{

}

} // hbs
} // mrdox
} // clang
