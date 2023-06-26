//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_LUAHANDLEBARS_HPP
#define MRDOX_API_SUPPORT_LUAHANDLEBARS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Lua.hpp>

namespace clang {
namespace mrdox {

/** Add the Handlebars Lua instance as a global
*/
MRDOX_DECL
Error
tryLoadHandlebars(
    lua::Context const& ctx);

} // mrdox
} // clang

#endif
