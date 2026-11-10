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
#include "ExtensionPaths.hpp"
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua.hpp>
#include <mrdocs/Generators/script/ScriptGenerator.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
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

// Install the `mrdocs` global carrying the `register_transform` and
// `register_generator` entry points before the chunk runs. The shared
// registrations pointer is carried as each closure's single upvalue.
void
registerLuaExtensionApi(lua::Context& ctx, LuaRegistrations& regs)
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
    lua_setglobal(L, "mrdocs");
}

// The searcher `addModuleDirectory` puts at `package.searchers[2]`. Upvalue
// 1 is the module directory as UTF-8 text and upvalue 2 is the searcher it
// replaced. `require "rules"` finds <directory>/rules.lua or, failing that,
// <directory>/rules/init.lua; a dotted name such as "a.b" is looked up as
// "a/b". The files are read through the UTF-8 path helpers, so a directory
// whose name has characters the ANSI code page lacks resolves on Windows too,
// which a `package.path` template handed to `fopen` would not. A name found
// in neither place goes to the replaced searcher, so `package.path` still
// resolves what the directory does not hold.
int
luaDirectorySearcher(lua_State* L)
{
    luaL_checkstring(L, 1);
    int status = LUA_OK;
    bool found = false;
    // Every object with a destructor lives in this block, which ends before
    // anything that raises a Lua error, because Lua's error jump does not run
    // destructors.
    {
        std::string name = lua_tostring(L, 1);
        for (char& c : name)
        {
            if (c == '.')
            {
                c = '/';
            }
        }
        std::string const directory = lua_tostring(L, lua_upvalueindex(1));
        std::string const candidates[2] = {
            directory + '/' + name + ".lua",
            directory + '/' + name + "/init.lua"};
        for (std::string const& candidate : candidates)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(utf8Path(candidate), ec))
            {
                continue;
            }
            found = true;
            auto text = files::getFileText(candidate);
            if (!text)
            {
                lua_pushfstring(L, "cannot read %s", candidate.c_str());
                status = LUA_ERRFILE;
                break;
            }
            std::string const chunkName = "@" + candidate;
            status = luaL_loadbuffer(
                L, text->data(), text->size(), chunkName.c_str());
            if (status == LUA_OK)
            {
                lua_pushlstring(L, candidate.data(), candidate.size());
            }
            break;
        }
    }
    if (!found)
    {
        lua_pushvalue(L, lua_upvalueindex(2));
        lua_pushvalue(L, 1);
        lua_call(L, 1, LUA_MULTRET);
        return lua_gettop(L) - 1;
    }
    if (status != LUA_OK)
    {
        return lua_error(L);
    }
    return 2;
}

// Make `require` look in `directory` before anywhere else: `require "rules"`
// finds <directory>/rules.lua or, failing that, <directory>/rules/init.lua.
// `package.path` is left as it is; the directory is served by a searcher
// installed at `package.searchers[2]`, the slot of the path searcher.
void
addModuleDirectory(lua::Context& ctx, std::string const& directory)
{
    lua_State* L = static_cast<lua_State*>(ctx.nativeState());
    lua_getglobal(L, "package");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "searchers");
        if (lua_istable(L, -1))
        {
            lua_pushlstring(L, directory.data(), directory.size());
            lua_rawgeti(L, -2, 2);
            lua_pushcclosure(L, &luaDirectorySearcher, 2);
            lua_rawseti(L, -2, 2);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

} // (anon)

Expected<LoadedExtensions>
loadLuaExtensions(
    std::string const& scriptPath,
    report::Reporter& reporter,
    std::string const& directory)
{
    // The engine is a shared handle from the start, so both the transforms
    // and the generators can keep it alive without a separate owner.
    auto vm = std::make_shared<lua::Context>();

    LuaRegistrations regs;
    regs.vm = vm;
    registerLuaExtensionApi(*vm, regs);
    if (!directory.empty())
    {
        addModuleDirectory(*vm, directory);
    }

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
        reporter.warn("extension '{}' registered nothing", scriptPath);
    }

    LoadedExtensions loaded;
    loaded.vm = std::move(vm);
    loaded.transforms = std::move(regs.transforms);
    loaded.generators = std::move(regs.generators);
    return loaded;
}

} // mrdocs
