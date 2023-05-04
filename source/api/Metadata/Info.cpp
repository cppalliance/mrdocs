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

#include <mrdox/Metadata/Info.hpp>
#include "api/Support/Debug.hpp"
#include <mrdox/Metadata/Record.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <cassert>

namespace clang {
namespace mrdox {

llvm::SmallString<16>
Info::
extractName() const
{
    if (!Name.empty())
        return Name;

    switch (IT)
    {
    case InfoType::IT_namespace:
        // Cover the case where the project contains a base namespace called
        // 'GlobalNamespace' (i.e. a namespace at the same level as the global
        // namespace, which would conflict with the hard-coded global namespace name
        // below.)
        if (Name == "GlobalNamespace" && Namespace.empty())
            return llvm::SmallString<16>("@GlobalNamespace");
        // The case of anonymous namespaces is taken care of in serialization,
        // so here we can safely assume an unnamed namespace is the global
        // one.
        return {}; //return llvm::SmallString<16>("GlobalNamespace");

    // VFALCO This API makes assumptions about what is
    //        valid in the output format. We could for
    //        example use base64 or base41...
    case InfoType::IT_record:
        return llvm::SmallString<16>("@nonymous_record_" +
            toHex(llvm::toStringRef(id)));
    case InfoType::IT_function:
        return llvm::SmallString<16>("@nonymous_function_" +
            toHex(llvm::toStringRef(id)));
    case InfoType::IT_enum:
        return llvm::SmallString<16>("@nonymous_enum_" +
            toHex(llvm::toStringRef(id)));
    case InfoType::IT_typedef:
        return llvm::SmallString<16>("@nonymous_typedef_" +
            toHex(llvm::toStringRef(id)));
    case InfoType::IT_variable:
        return llvm::SmallString<16>("@nonymous_var_" +
            toHex(llvm::toStringRef(id)));
    case InfoType::IT_default:
        return llvm::SmallString<16>("@nonymous_" +
            toHex(llvm::toStringRef(id)));
    default:
        llvm_unreachable("Invalid InfoType.");
        return llvm::SmallString<16>("");
    }
}

//------------------------------------------------


llvm::StringRef
Info::
getFullyQualifiedName(
    std::string& temp) const
{
    temp.clear();
    for(auto const& ns : llvm::reverse(Namespace))
    {
        temp.append(ns.Name.data(), ns.Name.size());
        temp.append("::");
    }
    auto s = extractName();
    temp.append(s.data(), s.size());
    return temp;
}

llvm::StringRef
Info::
symbolType() const noexcept
{
    switch(this->IT)
    {
    case InfoType::IT_default:
        return "default";
    case InfoType::IT_namespace:
        return "namespace";
    case InfoType::IT_record:
        return clang::TypeWithKeyword::getTagTypeKindName(
            static_cast<RecordInfo const*>(this)->TagType);
    case InfoType::IT_function:
        return "function";
    case InfoType::IT_enum:
        return "enum";
    case InfoType::IT_typedef:
        return "typedef";
    default:
        llvm_unreachable("unknown InfoType");
    }
}

} // mrdox
} // clang
