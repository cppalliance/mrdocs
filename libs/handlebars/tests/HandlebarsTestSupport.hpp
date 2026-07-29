//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_HANDLEBARS_SUPPORT_HPP
#define MRDOCS_TEST_HANDLEBARS_SUPPORT_HPP

// Shared helpers for the handlebars unit and golden test executables.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars.hpp>
#include <concepts>
#include <string>
#include <string_view>

namespace mrdocs {

// The engine lives in mrdocs::handlebars. The tests are a self-contained
// consumer context, so they pull the names they exercise into mrdocs:: here
// (rather than spelling out handlebars:: at every use site).
using handlebars::isEmpty;
using handlebars::Handlebars;
using handlebars::HandlebarsOptions;
using handlebars::OutputRef;
using handlebars::HTMLEscape;
using handlebars::createFrame;
namespace helpers = handlebars::helpers;

template <std::convertible_to<dom::Value> DomValue>
requires (!std::convertible_to<DomValue, std::string_view>)
std::string
HTMLEscape(
    DomValue const& val)
{
    dom::Value v = val;
    if (v.isString())
    {
        return HTMLEscape(v.getString().get());
    }
    if (v.isObject() && v.getObject().exists("toHTML"))
    {
        dom::Value fn = v.getObject().get("toHTML");
        if (fn.isFunction()) {
            return toString(fn.getFunction()());
        }
    }
    if (v.isNull() || v.isUndefined())
    {
        return {};
    }
    return toString(v);
}

} // mrdocs

#endif
