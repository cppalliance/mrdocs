//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ENGINES_LUA_SCOPE_HPP
#define MRDOCS_API_ENGINES_LUA_SCOPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua/Context.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>
#include <mrdocs/Engines/Lua/zstring.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/polyfill/source_location.hpp>
#include <string_view>

namespace mrdocs {
namespace lua {

/** Stack scope guard for Lua calls.
*/
/** Helper that balances the Lua stack for a Context scope.
*/
class Scope
{
    Context ctx_;
    std::size_t refs_;
    int top_;

    friend struct Access;

    void reset();

public:
    MRDOCS_DECL
    /** Create a scope that manages Lua stack references.
        @param ctx Lua context to guard.
    */
    Scope(Context const& ctx) noexcept;

    MRDOCS_DECL
    /** Pop any pending stack references on destruction.
    */
    ~Scope();

    /** Load a Lua chunk

        @param luaChunk The Lua chunk to load.
        @param chunkName The name of the chunk (used in error messages).
        @param loc The source location of the call site.
        @return The function if successful, or an error.
    */
    MRDOCS_DECL
    Expected<Function>
    loadChunk(
        std::string_view luaChunk,
        zstring chunkName,
        source_location loc =
            source_location::current());

    /** Load a Lua chunk

        @param luaChunk The Lua chunk to load.
        @param loc The source location of the call site.
        @return The function if successful, or an error.
    */
    MRDOCS_DECL
    Expected<Function>
    loadChunk(
        std::string_view luaChunk,
        source_location loc =
            source_location::current());

    /** Run a Lua chunk.

        @param fileName The name of the file to load.
        @param loc The source location of the call site.
        @return The function if successful, or an error.
    */
    MRDOCS_DECL
    Expected<Function>
    loadChunkFromFile(
        std::string_view fileName,
        source_location loc =
            source_location::current());

    /** Return the global table.
    */
    MRDOCS_DECL
    Table
    getGlobalTable();

    /** Return a value from the global table if it exists.

        @param key The key to get.
        @param loc The source location of the call site.
        @return The value if it exists, or an error.
    */
    MRDOCS_DECL
    Expected<Value>
    getGlobal(
        std::string_view key,
        source_location loc =
            source_location::current());

    /** Push a dom::Value onto the Lua stack.

        Primitives (nil, boolean, integer, string) are pushed as their
        Lua-native counterparts. Arrays and objects are pushed as
        userdata wrapping the underlying dom container, with the same
        lazy bindings used elsewhere in the wrapper.

        @param value The DOM value to push.
        @return A Value referring to the new stack slot.
    */
    MRDOCS_DECL
    Value
    pushDom(dom::Value const& value);
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_SCOPE_HPP
