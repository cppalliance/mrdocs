//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ReportDom.hpp"
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>
#include <utility>

namespace mrdocs {

namespace {

// Join call arguments into one message the way `console.log` does, so the
// functions of `ctx.report` accept free-form argument lists.
std::string
joinArgs(dom::Array const& args)
{
    std::string msg;
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            msg += ' ';
        }
        dom::Value const v = args.get(i);
        if (v.isString())
        {
            msg += std::string(v.getString());
        }
        else if (v.isObject() || v.isArray())
        {
            msg += dom::JSON::stringify(v);
        }
        else
        {
            msg += toString(v);
        }
    }
    return msg;
}

} // (anon)

dom::Value
reportDom(report::Reporter& reporter)
{
    dom::Object api;
    api.set("warn", dom::Value(dom::makeVariadicInvocable([&reporter](dom::Array const& args) -> Expected<dom::Value, dom::Error>
                        {
            reporter.warn("{}", joinArgs(args));
            return dom::Value(); })));
    api.set("error", dom::Value(dom::makeVariadicInvocable([&reporter](dom::Array const& args) -> Expected<dom::Value, dom::Error>
                         {
            reporter.error("{}", joinArgs(args));
            return dom::Value(); })));
    api.set("info", dom::Value(dom::makeVariadicInvocable([&reporter](dom::Array const& args) -> Expected<dom::Value, dom::Error>
                        {
            reporter.info("{}", joinArgs(args));
            return dom::Value(); })));
    return dom::Value(std::move(api));
}

} // mrdocs
