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

#ifndef MRDOX_META_REFERENCE_HPP
#define MRDOX_META_REFERENCE_HPP

#include <mrdox/meta/Symbols.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <utility>

namespace clang {
namespace mrdox {

struct Reference
{
    /** Unique identifier of the referenced symbol.
    */
    SymbolID USR = SymbolID();

    // Name of type (possibly unresolved). Not including namespaces or template
    // parameters (so for a std::vector<int> this would be "vector"). See also
    // QualName.
    llvm::SmallString<16> Name;

    /** The type of the referenced symbol.
    */
    InfoType RefType = InfoType::IT_default;

    // Path of directory where the generated file
    // will be saved (possibly unresolved)
    llvm::SmallString<128> Path;

    //--------------------------------------------

    // This variant (that takes no qualified name parameter) uses the Name as the
    // QualName (very useful in unit tests to reduce verbosity). This can't use an
    // empty string to indicate the default because we need to accept the empty
    // string as a valid input for the global namespace (it will have
    // "GlobalNamespace" as the name, but an empty QualName).
    Reference(
        SymbolID USR = EmptySID,
        llvm::StringRef Name = llvm::StringRef(),
        InfoType IT = InfoType::IT_default)
        : USR(USR)
        , Name(Name)
        , RefType(IT)
    {
    }

    Reference(
        SymbolID USR,
        llvm::StringRef Name,
        InfoType IT,
        llvm::StringRef Path)
        : USR(USR)
        , Name(Name)
        , RefType(IT)
        , Path(Path)
    {
    }

    bool
    operator==(
        Reference const& Other) const
    {
        // VFALCO Is this function only needed
        //        for the old unit tests?
        return
            std::tie(USR, Name, RefType) ==
            std::tie(Other.USR, Other.Name, Other.RefType);
    }

    bool canMerge(Reference const& Other);
    void merge(Reference&& I);

    /// Returns the path for this Reference relative to CurrentPath.
    llvm::SmallString<64> getRelativeFilePath(llvm::StringRef const& CurrentPath) const;

    /// Returns the basename that should be used for this Reference.
    //llvm::SmallString<16> getFileBaseName() const;
};

} // mrdox
} // clang

#endif
