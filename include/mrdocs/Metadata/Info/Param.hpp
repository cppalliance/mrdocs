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

#ifndef MRDOCS_API_METADATA_INFO_PARAM_HPP
#define MRDOCS_API_METADATA_INFO_PARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <string>

namespace clang::mrdocs {

/** Represents a single function parameter */
struct Param final
{
    /** The type of this parameter
     */
    Optional<Polymorphic<TypeInfo>> Type = std::nullopt;

    /** The parameter name.
     */
    Optional<std::string> Name;

    /** The default argument for this parameter, if any
      */
    Optional<std::string> Default;

    // KRYSTIAN TODO: attributes (nodiscard, deprecated, and carries_dependency)
    // KRYSTIAN TODO: flag to indicate whether this is a function parameter pack

    Param() = default;

    Param(
        Polymorphic<TypeInfo>&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {}

    auto
    operator<=>(Param const&) const  = default;
};

MRDOCS_DECL
void
merge(Param& I, Param&& Other);

/** Return the Param as a @ref dom::Value object.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Param const& p,
    DomCorpus const* domCorpus);

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_PARAM_HPP
