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

#ifndef MRDOCS_API_ENGINES_LUA_HPP
#define MRDOCS_API_ENGINES_LUA_HPP

// Umbrella header for the embedded Lua engine. Each class and free function
// has its own header under mrdocs/Engines/Lua/. The names live in the
// mrdocs::lua namespace.

#include <mrdocs/Engines/Lua/Context.hpp>
#include <mrdocs/Engines/Lua/Function.hpp>
#include <mrdocs/Engines/Lua/Param.hpp>
#include <mrdocs/Engines/Lua/Scope.hpp>
#include <mrdocs/Engines/Lua/String.hpp>
#include <mrdocs/Engines/Lua/Table.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>
#include <mrdocs/Engines/Lua/Value.hpp>
#include <mrdocs/Engines/Lua/registerHelper.hpp>
#include <mrdocs/Engines/Lua/zstring.hpp>

#endif // MRDOCS_API_ENGINES_LUA_HPP
