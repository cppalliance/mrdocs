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

#ifndef MRDOCS_API_ENGINES_LUA_TYPE_HPP
#define MRDOCS_API_ENGINES_LUA_TYPE_HPP

#include <vector>

namespace mrdocs {

/** Lua interop helpers for the optional scripting/backend integration.

    This namespace contains glue for pushing/popping values, registering
    functions, and safely executing snippets so embedders can enable Lua
    without duplicating binding code.
*/
namespace lua {

/** Internal tag granting access to lua internals.
*/
struct Access;

class Context;
class Scope;

class Table;
class Value;
class Function;

/** Pointer to a Lua-callable function returning Value.
*/
using FunctionPtr = Value (*)(std::vector<Value>);


/** Types of values.
*/
enum class Type
{
    /// The value is nil
    nil      = 0,
    /// The value is a boolean
    boolean  = 1,
    /// The value is a number
    number   = 3,
    /// The value is a string
    string   = 4,
    /// The value is a table
    table    = 5,
    /// The value is a function
    function = 6
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_TYPE_HPP
