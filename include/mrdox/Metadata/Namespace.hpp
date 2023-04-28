//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_NAMESPACE_HPP
#define MRDOX_METADATA_NAMESPACE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Scope.hpp>

namespace clang {
namespace mrdox {

/** Describes a namespace.
*/
struct NamespaceInfo
    : public Info
{
    Scope Children;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_namespace;

    NamespaceInfo();

    NamespaceInfo(
        SymbolID id,
        llvm::StringRef Name = llvm::StringRef());
};

} // mrdox
} // clang

#endif
