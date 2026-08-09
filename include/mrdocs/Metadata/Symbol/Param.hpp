//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_PARAM_HPP
#define MRDOCS_API_METADATA_SYMBOL_PARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Represents a single function parameter
*/
struct Param final
{
    /** The type of this parameter
    */
    Polymorphic<struct Type> Type = Polymorphic<struct Type>(AutoType{});

    /** The parameter name.
    */
    Optional<std::string> Name;

    /** The default argument for this parameter, if any
    */
    Optional<std::string> Default;

    /** Create an empty parameter with an `auto` type.
    */
    Param() = default;

    /** Construct a parameter with type, name, and default expression.
        @param type Parameter type.
        @param name Parameter identifier.
        @param def_arg Default argument expression, if present.
    */
    Param(
        Polymorphic<struct Type>&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {}

};

MRDOCS_DESCRIBE_STRUCT(
    Param,
    (),
    (Type, Name, Default)
)

/** Merge parameters element-wise, appending extras from `src`.

    @param dst The destination.
    @param src The source (moved from).
*/
MRDOCS_DECL
void
merge(std::vector<Param>& dst, std::vector<Param>&& src);


} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_PARAM_HPP
