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

#ifndef MRDOCS_API_ENGINES_LUA_PARAM_HPP
#define MRDOCS_API_ENGINES_LUA_PARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>

namespace mrdocs {
namespace lua {

/** A lazy container to push values to the Lua stack.
*/
class MRDOCS_DECL
    Param
{
    enum class Kind
    {
        nil,
        boolean,
        integer,
        string,
        value,
        domArray,
        domObject
    };

    Kind kind_;

    union
    {
        /** Stored boolean value when kind_ == boolean.
        */
        bool b_;
        /** Stored integer value when kind_ == integer.
        */
        int i_;
        /** Stack index when kind_ == value.
        */
        int index_; // for Value
        /** Stored string view when kind_ == string.
        */
        std::string_view s_;
        /** Stored array when kind_ == domArray.
        */
        dom::Array arr_;
        /** Stored object when kind_ == domObject.
        */
        dom::Object obj_;
    };

    friend struct Access;

    void push(Scope&) const;
    Param(Param&&) noexcept;

public:
    /** Destroy the stored value without throwing.
    */
    ~Param();

    /** Construct a nil parameter.
    */
    Param(std::nullptr_t) noexcept;
    /** Construct an integer parameter.
    */
    Param(std::int64_t) noexcept;
    /** Construct a string parameter (non-owning).
    */
    Param(std::string_view s) noexcept;
    /** Construct from a Lua Value already on the stack.
    */
    Param(Value const& value) noexcept;
    /** Construct from a DOM array.
    */
    Param(dom::Array arr) noexcept;
    /** Construct from a DOM object.
    */
    Param(dom::Object obj) noexcept;
    /** Construct from a generic DOM value.
    */
    Param(dom::Value const& value) noexcept;

    /** Deleted copy constructor to avoid double pops.
    */
    Param(Param const&) = delete;
    /** Deleted copy assignment to avoid double pops.
    */
    Param& operator=(Param const&) = delete;

    /** Construct a boolean parameter from a bool-like type.
    */
    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Param(Boolean const& b) noexcept
        : kind_(Kind::boolean)
        , b_(b)
    {
    }

    /** Construct a string parameter from C-string.
    */
    Param(char const* s) noexcept
        : Param(std::string_view(s))
    {
    }

    /** Construct a string parameter from a convertible string type.
    */
    template<class String>
    requires std::is_convertible_v<
        String, std::string_view>
    Param(String const& s)
        : Param(std::string_view(s))
    {
    }

    /** Construct an integral parameter from an enum.
    */
    template<class Enum>
    requires std::is_enum_v<Enum>
    Param(Enum v) noexcept
        : kind_(Kind::integer)
        , i_(static_cast<int>(v))
    {
    }
};

} // lua
} // mrdocs

#endif // MRDOCS_API_ENGINES_LUA_PARAM_HPP
