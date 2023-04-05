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

#ifndef MRDOX_JAD_REFERENCE_HPP
#define MRDOX_JAD_REFERENCE_HPP

#include "jad/Types.hpp"
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <utility>

namespace clang {
namespace mrdox {

struct Reference
{
    // This variant (that takes no qualified name parameter) uses the Name as the
    // QualName (very useful in unit tests to reduce verbosity). This can't use an
    // empty string to indicate the default because we need to accept the empty
    // string as a valid input for the global namespace (it will have
    // "GlobalNamespace" as the name, but an empty QualName).
    Reference(
        SymbolID USR = SymbolID(),
        llvm::StringRef Name = llvm::StringRef(),
        InfoType IT = InfoType::IT_default)
        : USR(USR)
        , Name(Name)
        , QualName(Name)
        , RefType(IT)
    {
    }

    Reference(
        SymbolID USR,
        llvm::StringRef Name,
        InfoType IT,
        llvm::StringRef QualName,
        llvm::StringRef Path = llvm::StringRef())
        : USR(USR)
        , Name(Name)
        , QualName(QualName)
        , RefType(IT)
        , Path(Path)
    {
    }

    bool operator==(const Reference& Other) const
    {
        return std::tie(USR, Name, QualName, RefType) ==
            std::tie(Other.USR, Other.Name, QualName, Other.RefType);
    }

    bool mergeable(const Reference& Other);
    void merge(Reference&& I);

    /// Returns the path for this Reference relative to CurrentPath.
    llvm::SmallString<64> getRelativeFilePath(llvm::StringRef const& CurrentPath) const;

    /// Returns the basename that should be used for this Reference.
    llvm::SmallString<16> getFileBaseName() const;

    SymbolID USR = SymbolID(); // Unique identifier for referenced decl

    // Name of type (possibly unresolved). Not including namespaces or template
    // parameters (so for a std::vector<int> this would be "vector"). See also
    // QualName.
    llvm::SmallString<16> Name;

    // Full qualified name of this type, including namespaces and template
    // parameter (for example this could be "std::vector<int>"). Contrast to
    // Name.
    llvm::SmallString<16> QualName;

    InfoType RefType = InfoType::IT_default; // Indicates the type of this
    // Reference (namespace, record, function, enum, default).

    // Path of directory where the mrdox generated file will be saved
    // (possibly unresolved)
    llvm::SmallString<128> Path;
};

} // mrdox
} // clang

#endif
