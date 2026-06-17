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

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs {

namespace {

// Backing state for an extension's `register_*` calls. Transforms are
// collected here and invoked once the chunk has run; generators are handed
// straight to the corpus, which owns them past this VM's lifetime. Each
// registered Lua function is anchored in the Lua registry (never a global)
// and exposed as a `dom::Function`. This struct rides as the
// `register_transform` / `register_generator` closures' single upvalue.
struct LuaRegistrations
{
    lua::Context* ctx = nullptr;
    CorpusImpl* corpus = nullptr;
    std::vector<dom::Function> transforms;
    // Generators are owned by the corpus, not held here, so this counts
    // them only to tell whether the script registered anything at all.
    std::size_t generators = 0;
};

// `mrdocs.register_transform(fn)`: anchor `fn` in the registry and record it as
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
            "mrdocs.register_transform: expected a function argument");
    }
    else
    {
        lua_pushvalue(L, 1);
        int const ref = luaL_ref(L, LUA_REGISTRYINDEX);
        regs->transforms.push_back(lua::makeCallable(*regs->ctx, ref));
    }
    return result;
}

// `mrdocs.register_generator(id, fn)`: anchor `fn` in the registry and
// hand it to the corpus under `id`. The corpus keeps it runnable until a
// generator is selected and run, long after this chunk's stack has
// unwound.
int
luaRegisterGenerator(lua_State* L)
{
    LuaRegistrations* regs = static_cast<LuaRegistrations*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    int result = 0;
    if (lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TFUNCTION)
    {
        result = luaL_error(L,
            "mrdocs.register_generator: expected (string id, function)");
    }
    else
    {
        std::size_t len = 0;
        char const* data = lua_tolstring(L, 1, &len);
        std::string id(data, len);
        lua_pushvalue(L, 2);
        int const ref = luaL_ref(L, LUA_REGISTRYINDEX);
        regs->corpus->registerScriptGenerator(
            std::move(id), lua::makeCallable(*regs->ctx, ref));
        ++regs->generators;
    }
    return result;
}

// Install the `mrdocs` global carrying the `register_transform` and
// `register_generator` entry points before the chunk runs. The shared
// registrations pointer is carried as each closure's single upvalue.
void
registerLuaExtensionApi(
    lua::Context& ctx, CorpusImpl& corpus, LuaRegistrations& regs)
{
    regs.ctx = &ctx;
    regs.corpus = &corpus;
    lua_State* L = static_cast<lua_State*>(ctx.nativeState());
    lua_newtable(L);
    lua_pushlightuserdata(L, &regs);
    lua_pushcclosure(L, &luaRegisterTransform, 1);
    lua_setfield(L, -2, "register_transform");
    lua_pushlightuserdata(L, &regs);
    lua_pushcclosure(L, &luaRegisterGenerator, 1);
    lua_setfield(L, -2, "register_generator");
    lua_setglobal(L, "mrdocs");
}

// Invoke one registered transform with the `ctx` object, tagging any
// failure with the script path for context.
Expected<void>
invokeTransform(
    dom::Function const& transform,
    dom::Value const& ctx,
    std::string const& scriptPath)
{
    Expected<dom::Value> invoked = transform.try_invoke(ctx);
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
    registerLuaExtensionApi(ctx, corpus, regs);

    lua::Scope scope(ctx);

    // Each transform receives one `ctx` object: `ctx.corpus` is the
    // navigable corpus (an array of per-symbol proxies plus
    // `get(id)` / `lookup(name)`), `ctx.config` the generation config.
    // Reads run through reflection on the live C++ Symbol; writes
    // (`ctx.corpus.symbols[i].name = "..."`) mutate that Symbol directly
    // through the `__newindex` metamethod's `dom::Object::set` path.
    dom::Value ctxValue = buildTransformContext(corpus);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    // Running the chunk's top-level code is what calls
    // `mrdocs.register_transform`; the chunk's own return value is unused.
    MRDOCS_TRY(chunk.call());

    // A discovered script that registers nothing is almost always a
    // mistake (a misspelled `mrdocs.register_transform` /
    // `mrdocs.register_generator`,
    // or a guard that skipped it), so flag it rather than silently doing
    // nothing.
    if (regs.transforms.empty() && regs.generators == 0)
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
            result = invokeTransform(transform, ctxValue, scriptPath);
        }
    }
    return result;
}

} // mrdocs
