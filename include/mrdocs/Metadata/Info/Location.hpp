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

#ifndef MRDOCS_API_METADATA_INFO_LOCATION_HPP
#define MRDOCS_API_METADATA_INFO_LOCATION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Nullable.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

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

    constexpr
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

/** nullable_traits specialization for Location.

    Semantics
    - The “null” (sentinel) state is any Location whose ShortPath is empty.
    - Creating a null value produces a Location with all fields defaulted
      and ShortPath empty.
    - Making an existing value null clears ShortPath and resets the other
      fields to their defaults.

    Rationale
    - This mirrors the old LocationEmptyPredicate, which treated an empty
      ShortPath as “empty/null.”
**/
template<>
struct nullable_traits<Location>
{
    static constexpr bool
    is_null(Location const& v) noexcept
    {
        return v.ShortPath.empty();
    }

    static constexpr Location
    null() noexcept
    {
        return Location{
            /*full_path*/   {},
            /*short_path*/  {},
            /*source_path*/ {},
            /*line*/        0u,
            /*documented*/  false
        };
    }

    static constexpr void
    make_null(Location& v) noexcept
    {
        v.FullPath.clear();
        v.ShortPath.clear();    // sentinel condition
        v.SourcePath.clear();
        v.LineNumber  = 0;
        v.Documented  = false;
    }
};

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_LOCATION_HPP
