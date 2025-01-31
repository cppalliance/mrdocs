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

#ifndef MRDOCS_API_METADATA_SOURCE_HPP
#define MRDOCS_API_METADATA_SOURCE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

enum class FileKind
{
    // File in the source directory
    Source,
    // File in a system include directory
    System,
    // File outside the source directory
    Other
};

MRDOCS_DECL
std::string_view
toString(FileKind kind);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FileKind kind)
{
    v = toString(kind);
}

struct MRDOCS_DECL
    Location
{
    /** The full file path
    */
    std::string FullPath;

    /** The file path relative to one of the search directories
    */
    std::string ShortPath;

    /** The file path relative to the source-root directory
     */
    std::string SourcePath;

    /** Line number within the file
    */
    unsigned LineNumber = 0;

    /** Whether this location has documentation.
    */
    bool Documented = false;

    //--------------------------------------------

    Location(
        std::string_view const full_path = {},
        std::string_view const short_path = {},
        std::string_view const source_path = {},
        unsigned const line = 0,
        bool const documented = false)
        : FullPath(full_path)
        , ShortPath(short_path)
        , SourcePath(source_path)
        , LineNumber(line)
        , Documented(documented)
    {
    }

    auto operator<=>(Location const&) const = default;
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Location const& loc);

struct LocationEmptyPredicate
{
    constexpr bool operator()(
        Location const& loc) const noexcept
    {
        return loc.ShortPath.empty();
    }
};

/** Like std::optional<Location>
*/
using OptionalLocation = Optional<Location, LocationEmptyPredicate>;

/** Stores source information for a declaration.
*/
struct MRDOCS_DECL
    SourceInfo
{
    /** Location where the entity was defined

        KRYSTIAN NOTE: this is used for entities which cannot be
        redeclared -- regardless of whether such a declaration is
        actually a definition (e.g. alias-declarations and
        typedef declarations are never definition).
    */
    OptionalLocation DefLoc;

    /** Locations where the entity was declared.

        This does not include the definition.
    */
    std::vector<Location> Loc;

    constexpr virtual ~SourceInfo() = default;

protected:
    SourceInfo() = default;
};

MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo const& Other);

MRDOCS_DECL
void
merge(SourceInfo& I, SourceInfo&& Other);

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SourceInfo const& I);

} // clang::mrdocs

#endif
