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

#include "Info.hpp"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <cassert>

namespace clang {
namespace mrdox {

bool
Info::
canMerge(
    Info const& Other)
{
    return
        IT == Other.IT &&
        USR == Other.USR;
}

void
Info::
mergeBase(
    Info&& Other)
{
    assert(canMerge(Other));
    if (USR == EmptySID)
        USR = Other.USR;
    if (Name == "")
        Name = Other.Name;
    if (Path == "")
        Path = Other.Path;
    if (Namespace.empty())
        Namespace = std::move(Other.Namespace);
    // Unconditionally extend the description,
    // since each decl may have a comment.
    std::move(
        Other.Description.begin(),
        Other.Description.end(),
        std::back_inserter(Description));
    llvm::sort(Description);
    auto Last = std::unique(
        Description.begin(), Description.end());
    Description.erase(Last, Description.end());
    if (javadoc.brief.empty())
        javadoc.brief = std::move(Other.javadoc.brief);
    if (javadoc.desc.empty())
        javadoc.desc = std::move(Other.javadoc.desc);
}

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
        return llvm::SmallString<16>("GlobalNamespace");
    case InfoType::IT_record:
        return llvm::SmallString<16>("@nonymous_record_" +
            toHex(llvm::toStringRef(USR)));
    case InfoType::IT_enum:
        return llvm::SmallString<16>("@nonymous_enum_" +
            toHex(llvm::toStringRef(USR)));
    case InfoType::IT_typedef:
        return llvm::SmallString<16>("@nonymous_typedef_" +
            toHex(llvm::toStringRef(USR)));
    case InfoType::IT_function:
        return llvm::SmallString<16>("@nonymous_function_" +
            toHex(llvm::toStringRef(USR)));
    case InfoType::IT_default:
        return llvm::SmallString<16>("@nonymous_" + toHex(llvm::toStringRef(USR)));
    }
    llvm_unreachable("Invalid InfoType.");
    return llvm::SmallString<16>("");
}

llvm::SmallString<64>
Info::
getRelativeFilePath(
    llvm::StringRef const& CurrentPath) const
{
    return calculateRelativeFilePath(
        IT, Path, extractName(), CurrentPath);
}

llvm::SmallString<16>
Info::
getFileBaseName() const
{
    if (IT == InfoType::IT_namespace)
        return llvm::SmallString<16>("index");
    return extractName();
}

//------------------------------------------------

llvm::SmallString<64>
calculateRelativeFilePath(
    InfoType const& Type,
    llvm::StringRef const& Path,
    llvm::StringRef const& Name,
    llvm::StringRef const& CurrentPath)
{
    namespace path = llvm::sys::path;

    llvm::SmallString<64> FilePath;

    if (CurrentPath != Path)
    {
        // iterate back to the top
        for (path::const_iterator I =
            path::begin(CurrentPath);
            I != path::end(CurrentPath); ++I)
            path::append(FilePath, "..");
        path::append(FilePath, Path);
    }

    // Namespace references have a Path to the parent namespace, but
    // the file is actually in the subdirectory for the namespace.
    if (Type == mrdox::InfoType::IT_namespace)
        path::append(FilePath, Name);

    return path::relative_path(FilePath);
}

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

} // mrdox
} // clang
