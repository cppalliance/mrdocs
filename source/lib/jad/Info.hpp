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

#ifndef MRDOX_JAD_INFO_HPP
#define MRDOX_JAD_INFO_HPP

#include "jad/Javadoc.hpp"
#include "jad/Reference.hpp"
#include "jad/Types.hpp"
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <array>
#include <vector>
#include <string>

namespace clang {
namespace mrdox {

/** Common properties of all symbols
*/
struct Info
{
    /** Unique identifier for the declaration.
    */
    SymbolID USR = SymbolID();

    /** Kind of declaration.
    */
    InfoType const IT = InfoType::IT_default;

    /** Unqualified name.
    */
    llvm::SmallString<16> Name;

    /** In-order List of parent namespaces.
    */
    llvm::SmallVector<Reference, 4> Namespace;

    // Comment description of this decl.
    Javadoc javadoc;
    std::vector<CommentInfo> Description;

    // Path of directory where the clang-doc
    // generated file will be saved
    llvm::SmallString<128> Path;          

    //--------------------------------------------

    virtual ~Info() = default;
    Info(Info const &Other) = delete;
    Info(Info&& Other) = default;

    explicit
    Info(
        InfoType IT = InfoType::IT_default,
        SymbolID USR = SymbolID(),
        llvm::StringRef Name = llvm::StringRef(),
        llvm::StringRef Path = llvm::StringRef())
        : USR(USR)
        , IT(IT)
        , Name(Name)
        , Path(Path)
    {
    }

    bool mergeable(const Info& Other);
    void mergeBase(Info&& I);

    llvm::SmallString<16> extractName() const;

    /// Returns the file path for this Info relative to CurrentPath.
    llvm::SmallString<64> getRelativeFilePath(llvm::StringRef const& CurrentPath) const;

    /// Returns the basename that should be used for this Info.
    llvm::SmallString<16> getFileBaseName() const;

    /** Return the fully qualified name.
    */
    llvm::StringRef
    getFullyQualifiedName(
        std::string& temp) const;
};

llvm::SmallString<64>
calculateRelativeFilePath(
    InfoType const& Type,
    llvm::StringRef const& Path,
    llvm::StringRef const& Name,
    llvm::StringRef const& CurrentPath);

} // mrdox
} // clang

#endif
