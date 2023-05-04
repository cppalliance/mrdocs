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

#include "Reduce.hpp"
#include "api/Support/Debug.hpp"
#include <mrdox/Metadata/Namespace.hpp>
#include <mrdox/Config.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

NamespaceInfo::
NamespaceInfo()
    : Info(
        InfoType::IT_namespace,
        EmptySID,
        "")
    , Children(true)
{
}

NamespaceInfo::
NamespaceInfo(
    SymbolID USR,
    llvm::StringRef Name)
    : Info(InfoType::IT_namespace, USR, Name)
    , Children(true)
{
}

} // mrdox
} // clang
