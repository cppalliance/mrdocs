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

#ifndef MRDOX_FUNCTIONS_HPP
#define MRDOX_FUNCTIONS_HPP

#include "FieldType.hpp"
#include "Function.hpp"
#include "List.hpp"
#include "Symbol.hpp"
#include "Template.hpp"
#include "Types.hpp"
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace clang {
namespace mrdox {

/// The string used for unqualified names
using UnqualifiedName = llvm::SmallString<16>;

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo : SymbolInfo
{
    static constexpr InfoType type_id = InfoType::IT_function;

    explicit
    FunctionInfo(
        SymbolID USR = SymbolID())
        : SymbolInfo(InfoType::IT_function, USR)
    {
    }

    void merge(FunctionInfo&& I);

    bool IsMethod = false; // Indicates whether this function is a class method.
    Reference Parent;      // Reference to the parent class decl for this method.
    TypeInfo ReturnType;   // Info about the return type of this function.
    llvm::SmallVector<FieldTypeInfo, 4> Params; // List of parameters.
    // Access level for this method (public, private, protected, none).
    // AS_public is set as default because the bitcode writer requires the enum
    // with value 0 to be used as the default.
    // (AS_public = 0, AS_protected = 1, AS_private = 2, AS_none = 3)
    AccessSpecifier Access = AccessSpecifier::AS_public;

    // Full qualified name of this function, including namespaces and template
    // specializations.
    SmallString<16> FullName;

    // When present, this function is a template or specialization.
    llvm::Optional<TemplateInfo> Template;
};

/// A list of zero or more functions
using FunctionInfos = std::vector<FunctionInfo>;

//------------------------------------------------

/** A list of overloads for a function.
*/
struct FunctionOverloads
    : List<FunctionInfo>
{
    /// The name of the function.
    UnqualifiedName name;

    void insert(FunctionInfo I);
    void merge(FunctionOverloads&& other);
    FunctionOverloads(FunctionInfo I);

    FunctionOverloads(
        FunctionOverloads&&) = default;
    FunctionOverloads& operator=(
        FunctionOverloads&&) = default;
};

//------------------------------------------------

/** A list of functions, each with possible overloads.
*/
struct FunctionList
    : List<FunctionOverloads>
{
    clang::AccessSpecifier access;

    void insert(FunctionInfo I);
    void merge(FunctionList&& other);

    FunctionList(
        FunctionList&&) noexcept = default;
    FunctionList(
        clang::AccessSpecifier access_ =
            clang::AccessSpecifier::AS_public) noexcept
        : access(access_)
    {
    }

private:
    iterator find(llvm::StringRef name) noexcept;
};

//------------------------------------------------

} // mrdox
} // clang

#endif
