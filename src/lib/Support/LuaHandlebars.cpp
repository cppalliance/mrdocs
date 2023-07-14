//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "LuaHandlebars.hpp"
#include <mrdox/Support/Lua.hpp>

namespace clang {
namespace mrdox {

// VFALCO:
// C Functions use "Error::Throw" for C++ errors
// Or should we push an error object onto the stack?

[[maybe_unused]]
static
lua::Value
escapeExpression(
    std::vector<lua::Value> args)
{
    if(args.empty())
        return lua::Value(); // nil
    // remove excess arguments
#if 0
    if(args.size() > 1)
        args.erase(args.begin() + 1, args.end());
#endif
    if(! args.front().isString())
        return std::move(args.front());
    //
    // TODO: escape the thing
    //
    return std::move(args.front());
}

Error
tryLoadHandlebars(
    lua::Context const& ctx)
{
    lua::Scope scope(ctx);
    auto G = scope.getGlobalTable();
    lua::Table hbs(scope);

    lua::Table utils(scope);
    //utils.set("escapeExpression", &escapeExpression);
    utils.set("isEmpty", "2");
    utils.set("extend", "3");
    utils.set("toString", "4");
    utils.set("isArray", "5");
    utils.set("isFunction", "6");
    utils.set("log", "7");

    hbs.set("Utils", utils);

    hbs.set("SafeString", "8");
    hbs.set("createFrame", "9");
    //hbs.set("escapeExpression", utils.get("escapeExpression"));

    G.set("Handlebars", hbs);
    return Error::success();



}

} // mrdox
} // clang
