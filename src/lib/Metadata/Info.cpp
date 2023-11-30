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

#include "lib/Support/Radix.hpp"
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdocs {

dom::String
toString(InfoKind kind) noexcept
{
    switch(kind)
    {
    case InfoKind::Namespace:
        return "namespace";
    case InfoKind::Record:
        return "record";
    case InfoKind::Field:
        return "field";
    case InfoKind::Function:
        return "function";
    case InfoKind::Enum:
        return "enum";
    case InfoKind::Typedef:
        return "typedef";
    case InfoKind::Variable:
        return "variable";
    case InfoKind::Specialization:
        return "specialization";
    case InfoKind::Friend:
        return "friend";
    case InfoKind::Enumerator:
        return "enumerator";
    case InfoKind::Guide:
        return "guide";
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // mrdocs
} // clang
