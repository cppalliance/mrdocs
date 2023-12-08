//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Name.hpp>

namespace clang {
namespace mrdocs {

dom::String toString(NameKind kind) noexcept
{
    switch(kind)
    {
    case NameKind::Identifier:
        return "identifier";
    case NameKind::Specialization:
        return "specialization";
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // mrdocs
} // clang
