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

#include "Support/Radix.hpp"
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

std::string
Info::
extractName() const
{
    if (!Name.empty())
        return Name;

    switch(Kind)
    {
    case InfoKind::Namespace:
        // Cover the case where the project contains a base namespace called
        // 'GlobalNamespace' (i.e. a namespace at the same level as the global
        // namespace, which would conflict with the hard-coded global namespace name
        // below.)
        if (Name == "GlobalNamespace" && Namespace.empty())
            return "@GlobalNamespace";
        // The case of anonymous namespaces is taken care of in serialization,
        // so here we can safely assume an unnamed namespace is the global
        // one.
        return {}; //return std::string("GlobalNamespace");

    // VFALCO This API makes assumptions about what is
    //        valid in the output format. We could for
    //        example use base64 or base41...
    case InfoKind::Record:
        return std::string("@nonymous_record_") +
            toBase16(id);
    case InfoKind::Function:
        return std::string("@nonymous_function_") +
            toBase16(id);
    case InfoKind::Enum:
        return std::string("@nonymous_enum_") +
            toBase16(id);
    case InfoKind::Typedef:
        return std::string("@nonymous_typedef_") +
            toBase16(id);
    case InfoKind::Variable:
        return std::string("@nonymous_var_") +
            toBase16(id);
    default:
        // invalid InfoKind
        MRDOX_UNREACHABLE();
    }
}

//------------------------------------------------
#if 0
std::string&
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
#endif

std::string_view
Info::
symbolType() const noexcept
{
    switch(this->Kind)
    {
    case InfoKind::Namespace:
        return "namespace";
    case InfoKind::Record:
        switch(static_cast<RecordInfo const*>(this)->KeyKind)
        {
        case RecordKeyKind::Struct:
            return "struct";
        case RecordKeyKind::Class:
            return "class";
        case RecordKeyKind::Union:
            return "union";
        default:
            // unknown RecordKeyKind
            MRDOX_UNREACHABLE();
        }
    case InfoKind::Function:
        return "function";
    case InfoKind::Enum:
        return "enum";
    case InfoKind::Typedef:
        return "typedef";
    default:
        // unknown InfoKind
        MRDOX_UNREACHABLE();
    }
}

} // mrdox
} // clang
