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

#ifndef MRDOCS_API_METADATA_NAMESPACE_HPP
#define MRDOCS_API_METADATA_NAMESPACE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/BitField.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Scope.hpp>
#include <vector>

namespace clang {
namespace mrdocs {

union NamespaceFlags
{
    BitFieldFullValue raw{.value=0u};

    BitFlag<0> isInline;
    BitFlag<1> isAnonymous;
};

/** Describes a namespace.
*/
struct NamespaceInfo
    : IsInfo<InfoKind::Namespace>
    , ScopeInfo
{
    NamespaceFlags specs;

    //--------------------------------------------

    explicit NamespaceInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
