//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_SOURCE_HPP
#define MRDOX_API_METADATA_SOURCE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/Optional.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

struct MRDOX_DECL
    Location
{
    /** Name of the file
    */
    std::string Filename;

    /** Line number within the file
    */
    int LineNumber;

    /** Whether the file is inside the source root directory
    */
    bool IsFileInRootDir;

    //--------------------------------------------

    Location(
        int line = 0,
        std::string_view filename = "",
        bool in_root_dir = false)
        : Filename(filename)
        , LineNumber(line)
        , IsFileInRootDir(in_root_dir)
    {
    }
};

/** Like std::optional<SymbolID>
*/
using OptionalLocation = Optional<Location,
    decltype([](const Location& loc) { return loc.Filename.empty(); })>;

/** Stores source information for a declaration.
*/
struct MRDOX_DECL
    SourceInfo
{
    /** Location where the entity was defined

        KRYSTIAN NOTE: this is used for entities which cannot be
        redeclared -- regardless of whether such a declaration is
        actually a definition (e.g. alias-declarations and
        typedef declarations are never definition).
    */
    OptionalLocation DefLoc;

    /** Locations where the entity was declared,

        This does not include the definition.
    */
    std::vector<Location> Loc;

protected:
    SourceInfo() = default;
};

} // mrdox
} // clang

#endif
