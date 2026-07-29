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

#ifndef MRDOCS_API_ENGINES_LUA_STRING_HPP
#define MRDOCS_API_ENGINES_LUA_STRING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Engines/Lua/Value.hpp>
#include <string_view>

namespace mrdocs {
namespace lua {

/** A Lua string.
*/
class String : public Value
{
    friend struct Access;

    String(int index, Scope&) noexcept;

public:
    /** Wrap an existing Lua value as a string.
    */
    MRDOCS_DECL String(Value value);
    /** Create a new Lua string from the given view.
    */
    MRDOCS_DECL explicit String(std::string_view s);

    /** Retrieve the underlying string view.
        @return View of the Lua string contents.
    */
    MRDOCS_DECL std::string_view get() const noexcept;

    /** Dereference to the underlying string view.
        @return View of the Lua string contents.
    */
    std::string_view operator*() const noexcept
    {
        return get();
    }

    /** Implicit conversion to string view.
    */
    operator std::string_view() const noexcept
    {
        return get();
    }
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_STRING_HPP
