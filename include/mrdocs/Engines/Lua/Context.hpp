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

#ifndef MRDOCS_API_ENGINES_LUA_CONTEXT_HPP
#define MRDOCS_API_ENGINES_LUA_CONTEXT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>
#include <memory>

namespace mrdocs {
namespace lua {

/** A reference to an instance of a Lua interpreter.
*/
class MRDOCS_DECL
    Context
{
    struct Impl;

    std::shared_ptr<Impl> impl_;

    friend struct Access;
    friend class Scope;

public:
    /** Copy assignment.
    */
    Context& operator=(Context const&) = delete;

    /** Destructor.
    */
    ~Context();

    /** Constructor.
    */
    Context();

    /** Constructor.
    */
    Context(Context const&) noexcept;

    /** Return the underlying `lua_State*`.

        Exposed as `void*` so callers don't have to drag `lua.h` into
        the public API. Cast to `lua_State*` at the use site. The state
        is owned by this Context and must not be `lua_close`d by the
        caller; use this only when the wrapper does not yet abstract
        the operation you need (for example, registering a native
        C function that the script can call as a global).
    */
    MRDOCS_DECL
    void*
    nativeState() const noexcept;
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_CONTEXT_HPP
