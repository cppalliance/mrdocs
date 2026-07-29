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

#ifndef MRDOCS_API_ENGINES_LUA_ZSTRING_HPP
#define MRDOCS_API_ENGINES_LUA_ZSTRING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>
#include <cstddef>
#include <string_view>

namespace mrdocs {
namespace lua {

/** A null-terminated string.
*/
class zstring
{
    std::string s_;
    char const* c_str_;

public:
    /** Construct from a C-string pointer.
        @param s Null-terminated string.
    */
    zstring(char const* s) noexcept
        : c_str_(s)
    {
    }

    /** Construct from string_view (stores an owned copy).
        @param s String view to copy.
    */
    zstring(std::string_view s)
        : s_(s)
        , c_str_(s_.c_str())
    {
    }

    /** Construct from std::string without copying.
        @param s Source string.
    */
    zstring(std::string const& s)
        : c_str_(s.c_str())
    {
    }

    /** Return the underlying C-string pointer.
    */
    char const* c_str() const noexcept
    {
        return c_str_;
    }
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_ZSTRING_HPP
