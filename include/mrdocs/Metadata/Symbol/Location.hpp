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

#ifndef MRDOCS_API_METADATA_SYMBOL_LOCATION_HPP
#define MRDOCS_API_METADATA_SYMBOL_LOCATION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Nullable.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Support/CompareReflectedType.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <string>

namespace mrdocs {

class DomCorpus;

/** Source location of a symbol or entity.
*/
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

    /** Column number within the line
    */
    unsigned ColumnNumber = 0;

    /** Whether this location has documentation.
    */
    bool Documented = false;

    //--------------------------------------------

    /** Construct a location with optional fields.
        @param full_path Absolute path to the file on disk.
        @param short_path Repository- or search-root relative path, may be empty.
        @param source_path Path relative to the documented source root.
        @param line Line number within the file.
        @param col Column number within the line.
        @param documented Whether the location already carries user documentation.
    */
    constexpr
    Location(
        std::string_view const full_path = {},
        std::string_view const short_path = {},
        std::string_view const source_path = {},
        unsigned const line = 0,
        unsigned const col = 0,
        bool const documented = false)
        : FullPath(full_path)
        , ShortPath(short_path)
        , SourcePath(source_path)
        , LineNumber(line)
        , ColumnNumber(col)
        , Documented(documented)
    {
    }

};

MRDOCS_DESCRIBE_STRUCT(
    Location,
    (),
    (ShortPath, SourcePath, LineNumber, ColumnNumber, Documented)
)

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
*/
template<>
struct nullable_traits<Location>
{
    /** Test if the location is null (empty ShortPath).
        @return True when `ShortPath` is empty.
    */
    static constexpr bool
    is_null(Location const& v) noexcept
    {
        return v.ShortPath.empty();
    }

    /** Create a null location sentinel.
        @return Location with every field cleared.
    */
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

    /** Reset a location to the null sentinel state.
    */
    static constexpr void
    make_null(Location& v) noexcept
    {
        v.FullPath.clear();
        v.ShortPath.clear();    // sentinel condition
        v.SourcePath.clear();
        v.LineNumber  = 0;
        v.ColumnNumber = 0;
        v.Documented  = false;
    }
};

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_LOCATION_HPP
