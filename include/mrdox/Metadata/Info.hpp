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

#ifndef MRDOX_METADATA_INFO_HPP
#define MRDOX_METADATA_INFO_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

/** Common properties of all symbols
*/
struct Info
{
    /** The unique identifier for this symbol.
    */
    SymbolID id = SymbolID::zero;

    /** Kind of declaration.
    */
    InfoType const IT = InfoType::IT_default;

    /** The unqualified name.
    */
    std::string Name;

    /** In-order List of parent namespaces.
    */
    llvm::SmallVector<Reference, 4> Namespace;

    /** The extracted javadoc for this declaration.
    */
    std::unique_ptr<Javadoc> javadoc;

    //--------------------------------------------

    virtual ~Info() = default;
    Info(Info const &Other) = delete;
    Info(Info&& Other) = default;

    explicit
    Info(
        InfoType IT = InfoType::IT_default,
        SymbolID id_ = SymbolID::zero,
        llvm::StringRef Name = llvm::StringRef())
        : id(id_)
        , IT(IT)
        , Name(Name)
    {
    }

    //
    // Observers
    //

    MRDOX_DECL
    std::string
    extractName() const;

    /** Return the fully qualified name.
    */
    MRDOX_DECL
    std::string&
    getFullyQualifiedName(
        std::string& temp) const;

    /** Return a string representing the symbol type.

        For example, "namespace", "class", et. al.
    */
    llvm::StringRef
    symbolType() const noexcept;
};

} // mrdox
} // clang

#endif
