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

#ifndef MRDOCS_API_ENGINES_LUA_REGISTERHELPER_HPP
#define MRDOCS_API_ENGINES_LUA_REGISTERHELPER_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua/Context.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string_view>

namespace mrdocs { namespace handlebars { class Handlebars; } }

namespace mrdocs {
namespace lua {

/** Register a Lua helper function

    Register a Lua chunk as a Handlebars helper. The chunk source is
    resolved to a callable in the following order:

    1. **Chunk return value** - load and execute the chunk; if it returns
       a function, use that. This is the idiomatic shape for a per-file
       helper:
       Example: `return function(x) return 'lua:' .. tostring(x) end`

    2. **Global lookup** - if the chunk does not return a function, look
       up the helper name on the global table. This handles chunks that
       define a function as a side effect:
       Example: `function helper_name(x) return tostring(x) end`

    The resolved function is anchored in `LUA_REGISTRYINDEX` for the
    lifetime of the registration. When the helper is invoked from a
    template, positional arguments are converted from @ref dom::Value to
    Lua values; the trailing Handlebars options object is dropped (matching
    the JavaScript helper semantics) to avoid recursive marshalling of
    symbol contexts.

    @param hbs The Handlebars instance to register the helper into
    @param name The name of the helper function
    @param ctx The Lua context that anchors the helper closure
    @param script The Lua source that defines the helper function
    @return Success, or an error if the script could not be resolved to a function
*/
[[nodiscard]] MRDOCS_DECL
Expected<void, Error>
registerHelper(
    handlebars::Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script);

/** Wrap a registry-anchored Lua function as a dom::Function.

    The returned function invokes the referenced Lua function, marshalling
    arguments and result as DOM values, and owns the reference (released when
    the last copy is destroyed). Used by the extension API's
    `mrdocs.register_transform` / `register_generator`.

    @param ctx The Lua context whose registry holds the function and that the
        returned callable keeps alive.
    @param ref A `luaL_ref(L, LUA_REGISTRYINDEX)` reference to a function value
        in `ctx`'s state.
    @return A dom::Function that calls the anchored Lua function.
*/
[[nodiscard]] MRDOCS_DECL
dom::Function
makeCallable(Context ctx, int ref);

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_REGISTERHELPER_HPP
