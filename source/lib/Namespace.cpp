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

#include "Reduce.h"
//#include "Representation.h"
#include "Namespace.hpp"
#include <mrdox/Config.hpp>
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"

namespace clang {
namespace mrdox {

NamespaceInfo::
NamespaceInfo()
    : Info(
        InfoType::IT_namespace,
        EmptySID,
        "",
        "")
    // VFALCO Shouldn't this be AS_none? But
    //        the Bitcode writer expects the
    //        default to be AS_public...
    , Children(clang::AccessSpecifier::AS_public)
{
}

NamespaceInfo::
NamespaceInfo(
    SymbolID USR,
    StringRef Name,
    StringRef Path)
    : Info(InfoType::IT_namespace, USR, Name, Path)
    // VFALCO Shouldn't this be AS_none? But
    //        the Bitcode writer expects the
    //        default to be AS_public...
    , Children(clang::AccessSpecifier::AS_public)
{
}

void
NamespaceInfo::
merge(NamespaceInfo&& Other)
{
    assert(canMerge(Other));
    // Reduce children if necessary.
    reduceChildren(Children.Namespaces, std::move(Other.Children.Namespaces));
    reduceChildren(Children.Records, std::move(Other.Children.Records));
    reduceChildren(Children.Functions, std::move(Other.Children.Functions));
    reduceChildren(Children.Enums, std::move(Other.Children.Enums));
    reduceChildren(Children.Typedefs, std::move(Other.Children.Typedefs));
    mergeBase(std::move(Other));
}

} // mrdox
} // clang
