//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "LuaBinding.hpp"
#include "CorpusDom.hpp"

#include <lib/CorpusImpl.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>

#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs {

namespace {

// Collects the corpus transforms an extension declares through
// `register_transform`. Each registered Lua function is anchored in the
// Lua registry (never a global) and exposed as a `dom::Function` over the
// live corpus, so both scripting languages funnel through one call path.
// The collector rides as the `register_transform` closure's single
// upvalue.
struct LuaRegistrations
{
    lua::Context* ctx = nullptr;
    std::vector<dom::Function> transforms;
};

// `register_transform(fn)`: anchor `fn` in the registry and record it as
// a callable, so the host can invoke it once the chunk has run.
int
luaRegisterTransform(lua_State* L)
{
    LuaRegistrations* regs = static_cast<LuaRegistrations*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    int result = 0;
    if (lua_type(L, 1) != LUA_TFUNCTION)
    {
        result = luaL_error(L,
            "register_transform: expected a function argument");
    }
    else
    {
        lua_pushvalue(L, 1);
        int const ref = luaL_ref(L, LUA_REGISTRYINDEX);
        regs->transforms.push_back(lua::makeCallable(*regs->ctx, ref));
    }
    return result;
}

// Bind `register_transform` as the script-facing entry point before the
// chunk runs, the same way a script generator's `output` object is bound.
// The collector pointer is carried as the closure's single upvalue.
void
registerLuaExtensionApi(lua::Context& ctx, LuaRegistrations& regs)
{
    regs.ctx = &ctx;
    lua_State* L = static_cast<lua_State*>(ctx.nativeState());
    lua_pushlightuserdata(L, &regs);
    lua_pushcclosure(L, &luaRegisterTransform, 1);
    lua_setglobal(L, "register_transform");
}

// Invoke one registered transform with the corpus, tagging any failure
// with the script path for context.
Expected<void>
invokeTransform(
    dom::Function const& transform,
    dom::Value const& corpus,
    std::string const& scriptPath)
{
    Expected<dom::Value> invoked = transform.try_invoke(corpus);
    Expected<void> result;
    if (!invoked.has_value())
    {
        result = Unexpected(formatError(
            "extension '{}': {}",
            scriptPath, invoked.error().message()));
    }
    return result;
}

} // (anon)

Expected<void>
runOneLuaExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    lua::Context ctx;
    LuaRegistrations regs;
    registerLuaExtensionApi(ctx, regs);

    lua::Scope scope(ctx);

    // The corpus argument is a small navigable object: an array of
    // per-symbol proxies plus `get(id)` / `lookup(name)` functions.
    // Reads run through reflection on the live C++ Symbol; writes
    // (`sym.name = "..."`) mutate that Symbol directly through the
    // `__newindex` metamethod's `dom::Object::set` path.
    dom::Value corpusValue = buildCorpusDom(corpus);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    // Running the chunk's top-level code is what calls
    // `register_transform`; the chunk's own return value is unused.
    MRDOCS_TRY(chunk.call());

    // A discovered script that registers nothing is almost always a
    // mistake (a misspelled `register_transform`, or a guard that skipped
    // it), so flag it rather than silently doing nothing.
    if (regs.transforms.empty())
    {
        report::warn("extension '{}' registered nothing", scriptPath);
    }

    // Invoke each declared transform with the corpus, in registration
    // order, stopping at the first failure.
    Expected<void> result;
    for (dom::Function const& transform : regs.transforms)
    {
        if (result.has_value())
        {
            result = invokeTransform(transform, corpusValue, scriptPath);
        }
    }
    return result;
}

} // mrdocs
