//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Reflection/EnumToString.hpp>
#include <mrdocs/Metadata/Symbol/RecordKeyKind.hpp>

namespace mrdocs {

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordKeyKind kind)
{
    v = toString(kind);
}

} // mrdocs

