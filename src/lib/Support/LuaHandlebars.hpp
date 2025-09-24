//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_LUAHANDLEBARS_HPP
#define MRDOCS_LIB_SUPPORT_LUAHANDLEBARS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Lua.hpp>

namespace clang {
namespace mrdocs {

/** Add the Handlebars Lua instance as a global
*/
MRDOCS_DECL
Expected<void>
tryLoadHandlebars(
    lua::Context const& ctx);

} // mrdocs
} // clang

#endif // MRDOCS_LIB_SUPPORT_LUAHANDLEBARS_HPP
