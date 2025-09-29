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

#include <mrdocs/Metadata/DocComment/Block.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Support/String.hpp>

namespace mrdocs {
namespace doc {

std::strong_ordering
operator<=>(Polymorphic<Block> const& lhs, Polymorphic<Block> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

}
} // mrdocs