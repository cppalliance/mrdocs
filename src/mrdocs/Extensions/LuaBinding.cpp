//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "LuaBinding.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Generators/script/ScriptGenerator.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs {

namespace {

// Backing state for an extension's `register_*` calls, carried as each
// closure's single upvalue. Transforms are collected for the host to
// invoke later; generators are turned into self-contained ScriptGenerators
// that hold `vm` (the engine handle). Each registered Lua function is
// anchored in the Lua registry (never a global) and exposed as a
// `dom::Function`.
struct LuaRegistrations
{
    lua::Context* ctx = nullptr;
    std::shared_ptr<void> vm;
    std::vector<std::pair<std::string, dom::Function>> transforms;
    std::vector<std::unique_ptr<Generator>> generators;
};

// `mrdocs.register_transform(id, fn)`: anchor `fn` in the registry and
// record it under `id`, so the host can invoke it later and hand it the
// matching `transform-options.<id>` as `ctx.params`.
int
luaRegisterTransform(lua_State* L)
{
    LuaRegistrations* regs = static_cast<LuaRegistrations*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    int result = 0;
    if (lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TFUNCTION)
    {
        result = luaL_error(L,
            "mrdocs.register_transform: expected (string id, function)");
    }
    else
    {
        std::size_t len = 0;
        char const* data = lua_tolstring(L, 1, &len);
        std::string id(data, len);
        lua_pushvalue(L, 2);
        int const ref = luaL_ref(L, LUA_REGISTRYINDEX);
        regs->transforms.emplace_back(
            std::move(id), lua::makeCallable(*regs->ctx, ref));
    }
    return result;
}

// `mrdocs.register_generator(id, fn)`: anchor `fn` in the registry and
// wrap it as a self-contained ScriptGenerator that carries a strong handle
// to this engine, so it stays runnable long after this chunk's stack has
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
        regs->generators.push_back(
            std::make_unique<script::ScriptGenerator>(
                std::move(id), lua::makeCallable(*regs->ctx, ref), regs->vm));
    }
    return result;
}

// Join every call argument into one message (each converted with the same
// rules as `tostring`), so `mrdocs.warn`/`mrdocs.error` accept free-form
// argument lists.
std::string
luaJoinArgs(lua_State* L)
{
    std::string msg;
    int const n = lua_gettop(L);
    for (int i = 1; i <= n; ++i)
    {
        if (i > 1)
        {
            msg += ' ';
        }
        std::size_t len = 0;
        char const* data = luaL_tolstring(L, i, &len);
        msg.append(data, len);
        lua_pop(L, 1);
    }
    return msg;
}

// `mrdocs.report.warn(...)` / `mrdocs.report.error(...)`: report a diagnostic
// through the host's report system so it is formatted, counted, and subject to
// warn-as-error, rather than merely printed to the console. `warn` carries the
// run's `warn-as-error` flag as its single upvalue, so it emits at error level
// when the setting is on, matching how the rest of Mr.Docs escalates its own
// warnings.
int
luaWarn(lua_State* L)
{
    bool const warnAsError = lua_toboolean(L, lua_upvalueindex(1)) != 0;
    report::log(
        warnAsError ? report::Level::error : report::Level::warn,
        "{}", luaJoinArgs(L));
    return 0;
}

int
luaError(lua_State* L)
{
    report::error("{}", luaJoinArgs(L));
    return 0;
}

// Install the `mrdocs` global carrying the `register_transform` and
// `register_generator` entry points before the chunk runs. The shared
// registrations pointer is carried as each closure's single upvalue.
void
registerLuaExtensionApi(
    lua::Context& ctx, LuaRegistrations& regs, bool warnAsError)
{
    regs.ctx = &ctx;
    lua_State* L = static_cast<lua_State*>(ctx.nativeState());
    lua_newtable(L);
    lua_pushlightuserdata(L, &regs);
    lua_pushcclosure(L, &luaRegisterTransform, 1);
    lua_setfield(L, -2, "register_transform");
    lua_pushlightuserdata(L, &regs);
    lua_pushcclosure(L, &luaRegisterGenerator, 1);
    lua_setfield(L, -2, "register_generator");
    // Reporting lives under `mrdocs.report` so the surface can grow without
    // crowding the top-level `mrdocs` API.
    lua_newtable(L);
    lua_pushboolean(L, warnAsError ? 1 : 0);
    lua_pushcclosure(L, &luaWarn, 1);
    lua_setfield(L, -2, "warn");
    lua_pushcclosure(L, &luaError, 0);
    lua_setfield(L, -2, "error");
    lua_setfield(L, -2, "report");
    lua_setglobal(L, "mrdocs");
}

} // (anon)

Expected<LoadedExtensions>
loadLuaExtensions(std::string const& scriptPath, Config const& config)
{
    // The engine is a shared handle from the start, so both the transforms
    // and the generators can keep it alive without a separate owner.
    auto vm = std::make_shared<lua::Context>();

    LuaRegistrations regs;
    regs.vm = vm;
    registerLuaExtensionApi(*vm, regs, config.warnAsError);

    lua::Scope scope(*vm);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    // Running the chunk's top-level code is what calls the
    // `mrdocs.register_*` entry points; the chunk's own return value is
    // unused.
    MRDOCS_TRY(chunk.call());

    if (regs.transforms.empty() && regs.generators.empty())
    {
        // A discovered script that registers nothing is almost always a
        // mistake (a misspelled `mrdocs.register_transform` /
        // `mrdocs.register_generator`, or a guard that skipped it), so flag
        // it rather than silently doing nothing.
        report::warn("extension '{}' registered nothing", scriptPath);
    }

    LoadedExtensions loaded;
    loaded.vm = std::move(vm);
    loaded.transforms = std::move(regs.transforms);
    loaded.generators = std::move(regs.generators);
    return loaded;
}

} // mrdocs
