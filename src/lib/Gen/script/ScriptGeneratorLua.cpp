//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ScriptRunner.hpp"
#include "OutputSink.hpp"

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs::script {

namespace {

// Lua adapter for `OutputSink::write`. On failure, the script aborts via
// `luaL_error`; the host turns that into an `Unexpected` when `lua_pcall`
// returns non-OK. The sink pointer is carried as the closure's single
// upvalue.
int
luaWrite(lua_State* L)
{
    OutputSink* sink = static_cast<OutputSink*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    if (lua_type(L, 1) != LUA_TSTRING ||
        lua_type(L, 2) != LUA_TSTRING)
    {
        return luaL_error(L,
            "output.write: expected (string path, string contents)");
    }
    std::size_t pathLen = 0;
    char const* pathData = lua_tolstring(L, 1, &pathLen);
    std::size_t bodyLen = 0;
    char const* bodyData = lua_tolstring(L, 2, &bodyLen);

    Expected<void> result = sink->write(
        std::string_view(pathData, pathLen),
        std::string_view(bodyData, bodyLen));
    if (!result)
    {
        return luaL_error(L, "%s", result.error().message().c_str());
    }
    return 0;
}

// Build the `output` global table and bind its `write` method.
//
// We register the C closure directly on the raw `lua_State` (via the
// `Context::nativeState()` escape hatch) because the wrapper cannot carry
// a native callable through a DOM value: `domValue_push` has no function
// case. The closure carries the sink pointer as its single upvalue.
void
registerLuaOutputApi(lua_State* L, OutputSink& sink)
{
    lua_newtable(L);

    lua_pushlightuserdata(L, &sink);
    lua_pushcclosure(L, &luaWrite, 1);
    lua_setfield(L, -2, "write");

    lua_setglobal(L, "output");
}

} // (anon)

Expected<void>
runLuaGenerator(
    dom::Value const& corpus,
    std::string const& scriptPath,
    OutputSink& sink,
    dom::Value const& config,
    dom::Value const& params)
{
    lua::Context ctx;

    // Register the `output` global before loading the script so
    // top-level code can reference it, and so we can pass it as the
    // second argument below.
    registerLuaOutputApi(
        static_cast<lua_State*>(ctx.nativeState()), sink);

    lua::Scope scope(ctx);
    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    Expected<lua::Value> chunkResult = chunk.call();
    if (!chunkResult)
    {
        return Unexpected(chunkResult.error());
    }

    // Fetch the `output` global so it can be passed as the second
    // argument. It must outlive the `generate` call below, so hold it
    // here rather than moving it out.
    Expected<lua::Value> output = scope.getGlobal("output");
    if (!output)
    {
        return Unexpected(output.error());
    }

    auto callGenerate =
        [&](lua::Function&& fn) -> Expected<void>
        {
            Expected<lua::Value> result =
                fn.call(corpus, *output, config, params);
            if (!result)
            {
                return Unexpected(formatError(
                    "generator '{}': {}",
                    scriptPath, result.error().message()));
            }
            return {};
        };

    // A generator must define a global `generate` function, the same
    // shape JavaScript requires. Accepting only the named global (rather
    // than also a function the chunk returns) keeps one convention across
    // both languages and leaves room for a script to expose more than one
    // named entry point later.
    Expected<lua::Value> generateFn = scope.getGlobal("generate");
    if (!generateFn || !generateFn->isFunction())
    {
        return Unexpected(formatError(
            "generator '{}': script must define a 'generate' function",
            scriptPath));
    }
    return callGenerate(lua::Function(std::move(*generateFn)));
}

} // mrdocs::script
