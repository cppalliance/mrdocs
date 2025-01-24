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

#include <vector>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Scope.hpp>

namespace clang::mrdocs {

/** Describes a namespace.
*/
struct NamespaceInfo final
    : InfoCommonBase<InfoKind::Namespace>
    , ScopeInfo
{
    bool IsInline = false;
    bool IsAnonymous = false;

    /** Namespaces nominated by using-directives.
    */
    std::vector<SymbolID> UsingDirectives;

    //--------------------------------------------

    explicit NamespaceInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
