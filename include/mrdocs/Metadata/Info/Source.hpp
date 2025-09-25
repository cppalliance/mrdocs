//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INFO_SOURCE_HPP
#define MRDOCS_API_METADATA_INFO_SOURCE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/Location.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

/** Stores source information for a declaration.
*/
struct MRDOCS_DECL
    SourceInfo
{
    constexpr SourceInfo() = default;

    /** Location where the entity was defined

        KRYSTIAN NOTE: this is used for entities which cannot be
        redeclared -- regardless of whether such a declaration is
        actually a definition (e.g. alias-declarations and
        typedef declarations are never definition).
    */
    Optional<Location> DefLoc;

    /** Locations where the entity was declared.

        This does not include the definition.
    */
    std::vector<Location> Loc;

    constexpr virtual ~SourceInfo() = default;

    auto operator<=>(SourceInfo const&) const = default;
};

MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo const& Other);

MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo&& Other);

MRDOCS_DECL
Optional<Location>
getPrimaryLocation(SourceInfo const& I, bool preferDefinition);

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SourceInfo const& I);

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_SOURCE_HPP
