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

#ifndef MRDOCS_API_ENGINES_LUA_TABLE_HPP
#define MRDOCS_API_ENGINES_LUA_TABLE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua/Scope.hpp>
#include <mrdocs/Engines/Lua/Value.hpp>

namespace mrdocs {
namespace lua {

/** A Lua table.
*/
class Table : public Value
{
    friend struct Access;

    Table(int index, Scope&);

    Expected<Value>
    callImpl(
        std::string_view name,
        Param const* data,
        std::size_t size) const;

public:
    /** Construct a table by copying fields from a DOM object.
    */
    MRDOCS_DECL Table(Scope& scope, dom::Object const& obj);
    /** Wrap an existing Lua value as a table.
    */
    MRDOCS_DECL Table(Value value);
    /** Create an empty table in the given scope.
    */
    MRDOCS_DECL explicit Table(Scope& scope);

    //MRDOCS_DECL Value get(zstring key) const;

    /** Retrieve a table entry by key; returns nil if missing.
        @param key Table key to look up.
        @return Value stored at key or nil if absent.
    */
    MRDOCS_DECL
    Value
    get(
        std::string_view key) const;

    /** Create or replace the value with a key.

        @param key The key to set.
        @param value The value to set.
    */
    MRDOCS_DECL
    void
    set(
        std::string_view key,
        Param value) const;
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_TABLE_HPP
