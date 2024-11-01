//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Source.hpp>
#include "lib/Dom/LazyObject.hpp"
#include "lib/Dom/LazyArray.hpp"

namespace clang {
namespace mrdocs {

std::string_view
toString(FileKind kind)
{
    switch(kind)
    {
    case FileKind::Source:
        return "source";
    case FileKind::System:
        return "system";
    case FileKind::Other:
        return "other";
    default:
        MRDOCS_UNREACHABLE();
    };
}

namespace dom {
    template<>
    struct MappingTraits<Location>
    {
        template <class IO>
        void map(IO &io, Location const& loc) const
        {
            io.map("path", loc.Path);
            io.map("file", loc.Filename);
            io.map("line", loc.LineNumber);
            io.map("kind", loc.Kind);
            io.map("documented", loc.Documented);
        }
    };


    template<>
    struct MappingTraits<SourceInfo>
    {
        template <class IO>
        void map(IO &io, SourceInfo const& I) const
        {
            if (I.DefLoc)
            {
                io.map("def", *I.DefLoc);
            }
            if (!I.Loc.empty())
            {
                io.map("decl", dom::newArray<dom::LazyArrayImpl<std::vector<Location>>>(I.Loc));
            }
        }
    };

}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Location const& loc)
{
    v = dom::newObject<dom::LazyObjectImpl<Location>>(loc);
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SourceInfo const& I)
{
    v = dom::newObject<dom::LazyObjectImpl<SourceInfo>>(I);
}

} // mrdocs
} // clang
