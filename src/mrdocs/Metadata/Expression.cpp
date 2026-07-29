//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Expression.hpp>

namespace mrdocs {

void
merge(ExprInfo& I, ExprInfo&& Other)
{
    if (I.Written.empty())
    {
        I.Written = std::move(Other.Written);
    }
}

} // mrdocs
