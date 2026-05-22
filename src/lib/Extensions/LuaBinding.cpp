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
#include "SetMember.hpp"

#include <lib/CorpusImpl.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs {
namespace {

ExtensionState&
upvalueState(lua_State* L)
{
    ExtensionState* p = static_cast<ExtensionState*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    MRDOCS_ASSERT(p != nullptr);
    return *p;
}

// Pull a Lua stack value at `index` into a `dom::Value`. Tables are
// classified by inspection: any string key turns the table into a
// `dom::Object`; otherwise it's treated as a 1-based array (Lua
// convention) and copied entry-by-entry into a `dom::Array`.
// Unrepresentable Lua types (function, thread, light userdata) become
// `nil` so `assignFromDom` produces a typed error rather than this
// adapter swallowing the argument.
dom::Value
luaValueToDom(lua_State* L, int index);

dom::Value
luaTableToDom(lua_State* L, int absIdx)
{
    bool hasStringKey = false;
    lua_pushnil(L);
    while (lua_next(L, absIdx) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            hasStringKey = true;
            lua_pop(L, 2);
            break;
        }
        lua_pop(L, 1);
    }

    if (hasStringKey)
    {
        dom::Object obj;
        lua_pushnil(L);
        while (lua_next(L, absIdx) != 0)
        {
            if (lua_type(L, -2) == LUA_TSTRING)
            {
                std::size_t klen = 0;
                char const* kdata = lua_tolstring(L, -2, &klen);
                obj.set(
                    std::string_view(kdata, klen),
                    luaValueToDom(L, -1));
            }
            lua_pop(L, 1);
        }
        return dom::Value(std::move(obj));
    }

    dom::Array arr;
    lua_Unsigned const len = lua_rawlen(L, absIdx);
    for (lua_Unsigned i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, absIdx, static_cast<lua_Integer>(i));
        arr.push_back(luaValueToDom(L, -1));
        lua_pop(L, 1);
    }
    return dom::Value(std::move(arr));
}

dom::Value
luaValueToDom(lua_State* L, int index)
{
    int const absIdx = lua_absindex(L, index);
    switch (lua_type(L, absIdx))
    {
    case LUA_TNIL:
        // Lua has only one nullary value (nil); map it to DOM `Null`
        // (not `Undefined`) so scripts can pass nil through
        // `mrdocs.set` to clear an `Optional<T>` field, matching the
        // JS side and the docs.
        return dom::Value(nullptr);
    case LUA_TBOOLEAN:
        return dom::Value(lua_toboolean(L, absIdx) != 0);
    case LUA_TNUMBER:
        if (lua_isinteger(L, absIdx))
        {
            return dom::Value(static_cast<std::int64_t>(
                lua_tointeger(L, absIdx)));
        }
        return dom::Value(static_cast<std::int64_t>(
            lua_tonumber(L, absIdx)));
    case LUA_TSTRING:
    {
        std::size_t len = 0;
        char const* data = lua_tolstring(L, absIdx, &len);
        return dom::Value(std::string(data, len));
    }
    case LUA_TTABLE:
        return luaTableToDom(L, absIdx);
    default:
        return dom::Value();
    }
}

// Lua adapter for `setMemberImpl`. On failure the script aborts via
// `luaL_error`; the host turns that into an `Unexpected` when
// `lua_pcall` returns non-OK.
int
luaSet(lua_State* L)
{
    ExtensionState& state = upvalueState(L);

    if (lua_type(L, 1) != LUA_TSTRING ||
        lua_type(L, 2) != LUA_TSTRING)
    {
        return luaL_error(L,
            "mrdocs.set: expected (string symbol_id, string field, value)");
    }

    std::size_t idLen = 0;
    char const* idData = lua_tolstring(L, 1, &idLen);
    std::size_t fieldLen = 0;
    char const* fieldData = lua_tolstring(L, 2, &fieldLen);

    Expected<dom::Value, Error> result = setMemberImpl(
        state,
        dom::Value(std::string(idData, idLen)),
        dom::Value(std::string(fieldData, fieldLen)),
        luaValueToDom(L, 3));
    if (!result)
    {
        return luaL_error(L, "%s", result.error().message().c_str());
    }

    return 0;
}

// Build the `mrdocs` global table and populate it with the setters.
//
// We register C closures directly on the raw `lua_State*` (via the
// `Context::nativeState()` escape hatch) because the wrapper does not
// yet abstract "set a global to a native function with carried state."
// The closure carries the `ExtensionState` pointer as its single
// upvalue.
void
registerLuaMrDocsApi(lua_State* L, ExtensionState& state)
{
    lua_newtable(L);

    lua_pushlightuserdata(L, &state);
    lua_pushcclosure(L, &luaSet, 1);
    lua_setfield(L, -2, "set");

    lua_setglobal(L, "mrdocs");
}

} // (anon)

Expected<void>
runOneLuaExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    lua::Context ctx;
    ExtensionState state{ &corpus, {} };

    // Build the corpus DOM and the `id` -> `Symbol*` map in one pass.
    DomCorpus domCorpus(corpus);
    dom::Value corpusValue = buildCorpusDom(corpus, domCorpus, state);

    // Register the `mrdocs` global before loading the script so utility
    // code at chunk top-level can reference it if it wants to.
    registerLuaMrDocsApi(
        static_cast<lua_State*>(ctx.nativeState()), state);

    // Load the chunk and execute it (defines globals, including
    // `transform_corpus` if the script uses the `function name(...)`
    // shape rather than a returned function).
    lua::Scope scope(ctx);
    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    Expected<lua::Value> chunkResult = chunk.call();
    if (!chunkResult)
    {
        return Unexpected(chunkResult.error());
    }

    // Resolve `transform_corpus`. Prefer the chunk's return value (the
    // `return function(...) ... end` idiom); fall back to a same-named
    // global (the `function name(...)` idiom). If neither yields a
    // function, the extension has nothing to do - silently skip (an
    // empty extension is valid).
    //
    // We can't pre-declare a `lua::Value` and assign into it because
    // `lua::Value`'s user-defined move ctor implicitly deletes copy
    // assignment, so we run the call inline in each branch instead.
    auto callTransform =
        [&](lua::Function&& fn) -> Expected<void>
        {
            Expected<lua::Value> result = fn.call(corpusValue);
            if (!result)
            {
                return Unexpected(formatError(
                    "extension '{}': {}",
                    scriptPath, result.error().message()));
            }
            return {};
        };

    if (chunkResult->isFunction())
    {
        return callTransform(lua::Function(std::move(*chunkResult)));
    }

    Expected<lua::Value> global = scope.getGlobal("transform_corpus");
    if (!global || !global->isFunction())
    {
        return {};
    }
    return callTransform(lua::Function(std::move(*global)));
}

} // mrdocs
