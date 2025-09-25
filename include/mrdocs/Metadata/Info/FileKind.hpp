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

#ifndef MRDOCS_API_METADATA_INFO_FILEKIND_HPP
#define MRDOCS_API_METADATA_INFO_FILEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Nullable.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

enum class FileKind
{
    /// File in the source directory
    Source,
    /// File in a system include directory
    System,
    /// File outside the source directory
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

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_FILEKIND_HPP
