//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Metadata/Record.hpp>

namespace clang {
namespace mrdox {

std::string_view
toString(RecordKeyKind kind) noexcept
{
    switch(kind)
    {
    case RecordKeyKind::Struct:
        return "struct";
    case RecordKeyKind::Class:
        return "class";
    case RecordKeyKind::Union:
        return "union";
    default:
        // unknown RecordKeyKind
        MRDOX_UNREACHABLE();
    }
}

} // mrdox
} // clang
