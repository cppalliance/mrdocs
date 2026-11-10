//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_CACHEDDESCRIBEDOBJECT_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_CACHEDDESCRIBEDOBJECT_HPP

#include "../DescribedToDom.hpp"
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

// A read-only `dom::Object` that projects the fields of a described
// `Context`. A script receives it as the parent of its `ctx`, under the keys
// the script and the host add (`dom::overlayObject(extras, object)`).
//
// The object owns a copy of the context (a handful of pointers, a string and
// a `dom::Object` handle) and projects the fields from that copy, so a script
// that keeps `ctx` in a global can read it after the call that received it
// has returned.
//
// The described projection builds the value of a field each time the field is
// read, and the corpus is a navigable object that costs a walk over every
// symbol to build. A script reads `ctx.corpus` over and over, so the value of
// a described field is built on first read and kept, which also gives the
// script one object for `ctx.corpus` and one for `ctx.config` as a plain
// object would. The object has no `$meta` key: it is not a corpus object
// whose type a script needs to tell.
template <class Context>
class CachedDescribedObject final
    : public dom::ObjectImpl
{
    Context context_;
    dom::Object described_;
    mutable std::map<std::string, dom::Value, std::less<>> cache_;

public:
    explicit
    CachedDescribedObject(Context const& context)
        : context_(context)
        , described_(describedToDom(std::as_const(context_)).getObject())
    {
    }

    dom::Value
    get(std::string_view key) const override
    {
        if (key == "$meta")
        {
            return dom::Value();
        }
        auto const it = cache_.find(key);
        if (it != cache_.end())
        {
            return it->second;
        }
        dom::Value value = described_.get(key);
        cache_.emplace(std::string(key), value);
        return value;
    }

    // The fields come from the context, so a write has nowhere to go. The
    // overlay a script sees takes its writes itself.
    void
    set(dom::String, dom::Value) override
    {
    }

    bool
    exists(std::string_view key) const override
    {
        return key != "$meta" && described_.exists(key);
    }

    std::size_t
    size() const override
    {
        std::size_t count = 0;
        describe::for_each_member<Context>([&](auto)
            {
            ++count; });
        return count;
    }

    bool
    visit(std::function<bool(dom::String, dom::Value)> fn) const override
    {
        bool more = true;
        describe::for_each_member<Context>([&](auto d)
            {
            if (!more)
            {
                return;
            }
            std::string const name =
                dom::detail::normalizeMemberName(d.name);
            more = fn(dom::String(name), get(name)); });
        return more;
    }
};

} // mrdocs

#endif
