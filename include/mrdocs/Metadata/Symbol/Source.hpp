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

#ifndef MRDOCS_API_METADATA_SYMBOL_SOURCE_HPP
#define MRDOCS_API_METADATA_SYMBOL_SOURCE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Symbol/Location.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs {

/** Stores source information for a declaration.
*/
struct MRDOCS_DECL
    SourceInfo
{
    /** Construct with no locations.
    */
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

    /** Polymorphic base needs a virtual destructor.
    */
    constexpr virtual ~SourceInfo() = default;

};

MRDOCS_DESCRIBE_STRUCT(
    SourceInfo,
    (),
    (DefLoc, Loc)
)

/** Merge the location sets, preferring existing def/primary.
*/
MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo const& Other);

/** Merge, transferring ownership from the right-hand side.
*/
MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo&& Other);

/** Choose the best location to display for a symbol.
    @param I Source info to examine.
    @param preferDefinition If true, favor a definition location.
    @return The preferred location if available.
*/
MRDOCS_DECL
Optional<Location>
getPrimaryLocation(SourceInfo const& I, bool preferDefinition);

namespace dom { struct LazyObjectMapTag; }

/** Map the SourceInfo to a lazy DOM object.
*/
template <class IO>
void tag_invoke(dom::LazyObjectMapTag, IO&, SourceInfo const&);

/** Serialize source locations into a DOM value.
*/
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SourceInfo const& I);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_SOURCE_HPP
