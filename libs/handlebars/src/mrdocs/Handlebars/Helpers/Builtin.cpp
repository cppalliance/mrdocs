//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/Builtin.hpp>
#include <mrdocs/Handlebars.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mrdocs {
namespace handlebars {
namespace helpers {

// This .ipp fragment is included from Handlebars.cpp within
// `namespace mrdocs { namespace handlebars { namespace helpers {`. Not a
// standalone header: it has no include guard and is compiled as part of the
// Handlebars translation unit.
//
// Core built-in helpers (if, unless, with, each, lookup, log, ...) and their registration.

dom::Expected<void>
if_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2)
    {
        return Unexpected(dom::Error("#if requires exactly one argument"));
    }

    dom::Value conditional = arguments.get(0);
    dom::Value options = arguments.get(1);
    dom::Value context = options.get("context");
    if (conditional.isFunction())
    {
        auto _hbs_try24 = conditional.getFunction().try_invoke(context); if (!_hbs_try24) return Unexpected(_hbs_try24.error()); conditional = *std::move(_hbs_try24);
    }

    if ((!options.lookup("hash.includeZero") && !conditional) || isEmpty(conditional)) {
        { auto _hbs_try25 = options.get("write_inverse").getFunction().try_invoke(context); if (!_hbs_try25) return Unexpected(_hbs_try25.error()); }
    }
    else
    {
        { auto _hbs_try26 = options.get("write").getFunction().try_invoke(context); if (!_hbs_try26) return Unexpected(_hbs_try26.error()); }
    }
    return {};
}

dom::Expected<void>
unless_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2) {
        return Unexpected(dom::Error("#unless requires exactly one argument"));
    }

    dom::Value options = arguments.get(1);
    dom::Value fn = options.get("fn");
    dom::Value inverse = options.get("inverse");
    options.set("fn", inverse);
    options.set("inverse", fn);
    dom::Value write = options.get("write");
    dom::Value write_inverse = options.get("write_inverse");
    options.set("write", write_inverse);
    options.set("write_inverse", write);
    dom::Array inv_arguments = arguments;
    inv_arguments.set(1, options);
    return if_fn(inv_arguments);
}

dom::Expected<void>
with_fn(dom::Array const& arguments)
{
    if (arguments.size() != 2) {
        return Unexpected(dom::Error("#with requires exactly one argument"));
    }

    dom::Value context = arguments.get(0);
    dom::Value options = arguments.get(1);
    if (context.isFunction()) {
        auto _hbs_try27 = context.getFunction().try_invoke(options.get("context")); if (!_hbs_try27) return Unexpected(_hbs_try27.error()); context = *std::move(_hbs_try27);
    }

    if (!isEmpty(context))
    {
        dom::Value data = options.get("data");
        if (data && options.get("ids"))
        {
            data = createFrame(data);
            data.set("contextPath", detail::appendContextPath(
                data.get("contextPath"), options.get("ids").get(0)));
        }
        dom::Array blockParams = {{context}};
        dom::Array blockParamPaths = {{data && data.get("contextPath")}};
        dom::Object cbOpt;
        cbOpt.set("data", data);
        cbOpt.set("blockParams", blockParams);
        cbOpt.set("blockParamPaths", blockParamPaths);
        { auto _hbs_try28 = options.get("write").getFunction().try_invoke(context, cbOpt); if (!_hbs_try28) return Unexpected(_hbs_try28.error()); }
    }
    else
    {
        { auto _hbs_try29 = options.get("write_inverse").getFunction().try_invoke(options.get("context")); if (!_hbs_try29) return Unexpected(_hbs_try29.error()); }
    }
    return {};
}

dom::Expected<void>
each_fn(dom::Value context, dom::Value const& options)
{
    if (!options)
    {
        return Unexpected(dom::Error("Must pass iterator to #each"));
    }

    dom::Value fn = options.get("write");
    dom::Value inverse = options.get("write_inverse");
    std::size_t i = 0;
    dom::Value data;
    std::string contextPath;

    if (options.get("data") && options.get("ids")) {
        contextPath = detail::appendContextPath(
            options.lookup("data.contextPath"), options.get("ids").get(0)) + '.';
    }

    if (context.isFunction()) {
        auto _hbs_try30 = context.getFunction().try_invoke(options.get("context")); if (!_hbs_try30) return Unexpected(_hbs_try30.error()); context = *std::move(_hbs_try30);
    }

    if (options.get("data")) {
        data = createFrame(options.get("data"));
    }

    auto execIteration = [&context, &data, &contextPath, &fn](
        dom::Value const& field, std::size_t index, dom::Value const& last)
        -> dom::Expected<dom::Value>
    {
        if (data)
        {
            data.set("key", field);
            data.set("index", index);
            data.set("first", index == 0);
            data.set("last", last.getBool());
        }

        if (!contextPath.empty())
        {
            data.set("contextPath", contextPath + field);
        }

        dom::Array blockParams = {{context.get(field), field}};
        dom::Array blockParamPaths = {{data && data.get("contextPath"), nullptr}};
        dom::Object cbOpt;
        cbOpt.set("data", data);
        cbOpt.set("blockParams", blockParams);
        cbOpt.set("blockParamPaths", blockParamPaths);
        return fn.getFunction().try_invoke(context.get(field), cbOpt);
    };

    bool const isJSObject = static_cast<bool>(
        context && (context.isObject() || context.isArray()));
    if (isJSObject)
    {
        if (context.isArray())
        {
            std::size_t const n = context.size();
            for (; i < n; ++i)
            {
                bool const isLast = i == n - 1;
                { auto _hbs_try31 = execIteration(i, static_cast<std::int64_t>(i), isLast); if (!_hbs_try31) return Unexpected(_hbs_try31.error()); }
            }
        }
        else if (context.isObject())
        {
            dom::Value priorKey;
            auto exp = context.getObject().visit([&](
                dom::String const& key, dom::Value const& value) -> dom::Expected<void>
            {
                if (!priorKey.isUndefined())
                {
                    { auto _hbs_try32 = execIteration(priorKey, i - 1, false); if (!_hbs_try32) return Unexpected(_hbs_try32.error()); }
                }
                priorKey = key;
                ++i;
                return {};
            });
            if (!exp)
            {
                return Unexpected(exp.error());
            }
            if (!priorKey.isUndefined())
            {
                { auto _hbs_try33 = execIteration(priorKey, i - 1, true); if (!_hbs_try33) return Unexpected(_hbs_try33.error()); }
            }
        }
    }

    if (i == 0)
    {
        { auto _hbs_try34 = inverse.getFunction().try_invoke(options.get("context")); if (!_hbs_try34) return Unexpected(_hbs_try34.error()); }
    }
    return {};
}

dom::Expected<dom::Value>
lookup_fn(dom::Value const& obj, dom::Value const& field, dom::Value const& options)
{
    if (!obj)
    {
        return obj;
    }
    return options.get("lookupProperty").getFunction().try_invoke(obj, field);
}

dom::Expected<void>
log_fn(dom::Array const& arguments)
{
    // {level, args}
    dom::Array args = {{dom::Value{}}};
    dom::Value options = arguments.back();
    std::size_t const n = arguments.size();
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        args.emplace_back(arguments.get(i));
    }

    dom::Value level = 1;
    dom::Value hash = options.get("hash");
    dom::Value data = options.get("data");
    if (hash.exists("level") && !hash.get("level").isNull())
    {
        level = options.lookup("hash.level");
    }
    else if (data.exists("level") && !data.get("level").isNull())
    {
        level = options.lookup("data.level");
    }
    args.set(0, level);
    { auto _hbs_try35 = options.get("log").getFunction().call(args); if (!_hbs_try35) return Unexpected(_hbs_try35.error()); }
    return {};
}

dom::Expected<dom::Value>
helper_missing_fn(dom::Array const& arguments)
{
    if (arguments.size() == 1)
    {
        return {};
    }

    return Unexpected(dom::Error(
        std::format(R"(Missing helper: "{}")", arguments.back().get("name"))));
}

dom::Expected<void>
block_helper_missing_fn(
    dom::Value const& context, dom::Value options)
{
    if (context == true)
    {
        { auto _hbs_try36 = options.get("write").getFunction().try_invoke(options.get("context")); if (!_hbs_try36) return Unexpected(_hbs_try36.error()); }
    }
    else if (
        context == false ||
        context.isNull() ||
        context.isUndefined())
    {
        { auto _hbs_try37 = options.get("write_inverse").getFunction().try_invoke(options.get("context")); if (!_hbs_try37) return Unexpected(_hbs_try37.error()); }
    }
    else if (context.isArray())
    {
        if (!context.empty())
        {
            if (options.get("ids"))
            {
                options.set("ids", dom::Array{{options.get("name")}});
            }
            { auto _hbs_try38 = each_fn(context, options); if (!_hbs_try38) return Unexpected(_hbs_try38.error()); }
        }
        else
        {
            { auto _hbs_try39 = options.get("write_inverse").getFunction().try_invoke(options.get("context")); if (!_hbs_try39) return Unexpected(_hbs_try39.error()); }
        }
    }
    else
    {
        dom::Object fnOpt;
        if (options.get("data") && options.get("ids"))
        {
            dom::Object data = createFrame(options.get("data"));
            data.set(
                "contextPath",
                detail::appendContextPath(data.get("contextPath"), options.get("name")));
            fnOpt.set("data", data);
        }
        { auto _hbs_try40 = options.get("write").getFunction().try_invoke(context, fnOpt); if (!_hbs_try40) return Unexpected(_hbs_try40.error()); }
    }
    return {};
}

void
registerBuiltinHelpers(Handlebars& hbs)
{
    hbs.registerHelper("if", dom::makeVariadicInvocable(if_fn));
    hbs.registerHelper("unless", dom::makeVariadicInvocable(unless_fn));
    hbs.registerHelper("with", dom::makeVariadicInvocable(with_fn));
    hbs.registerHelper("each", dom::makeInvocable(each_fn));
    hbs.registerHelper("lookup", dom::makeInvocable(lookup_fn));
    hbs.registerHelper("log", dom::makeVariadicInvocable(log_fn));
    hbs.registerHelper("helperMissing", dom::makeVariadicInvocable(helper_missing_fn));
    hbs.registerHelper("blockHelperMissing", dom::makeInvocable(block_helper_missing_fn));
}

void
registerAntoraHelpers(Handlebars& hbs)
{
    hbs.registerHelper("and", dom::makeVariadicInvocable(and_fn));
    hbs.registerHelper("detag", dom::makeInvocable(detag_fn));
    hbs.registerHelper("eq", dom::makeVariadicInvocable(eq_fn));
    hbs.registerHelper("increment", dom::makeInvocable(increment_fn));
    hbs.registerHelper("ne", dom::makeVariadicInvocable(ne_fn));
    hbs.registerHelper("not", dom::makeVariadicInvocable(not_fn));
    hbs.registerHelper("or", dom::makeVariadicInvocable(or_fn));
    hbs.registerHelper("year", dom::makeInvocable(year_fn));
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
