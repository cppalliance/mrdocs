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

#ifndef MRDOX_METADATA_FUNCTION_HPP
#define MRDOX_METADATA_FUNCTION_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Bits.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace clang {
namespace mrdox {

/** Bit constants used with function specifiers.
*/
enum class FnFlags0 : std::uint32_t
{
    // Function Decl

    isVariadic              = 0x00000001, // has a C-style "..." variadic 
    isVirtualAsWritten      = 0x00000002,
    isPure                  = 0x00000004,
    isDefaulted             = 0x00000008,
    isExplicitlyDefaulted   = 0x00000010,
    isDeleted               = 0x00000020,
    isDeletedAsWritten      = 0x00000040,
    isNoReturn              = 0x00000080,

    hasOverrideAttr         = 0x00000100,
    hasTrailingReturn       = 0x00000200,

    constexprKind           = 0x00000400 +
                              0x00000800,
    exceptionSpecType       = 0x00001000 +
                              0x00002000 +
                              0x00004000 +
                              0x00008000,
    overloadedOperator      = 0x00010000 +
                              0x00020000 +
                              0x00040000 +
                              0x00080000 +
                              0x00100000 +
                              0x00200000,

    storageClass            = 0x00400000 +
                              0x00800000 +
                              0x01000000,

    // CXXMethodDecl

    isConst                 = 0x02000000,
    isVolatile              = 0x04000000,

    refQualifier            = 0x08000000 +
                              0x10000000
};

/** Bit constants used with function specifiers.
*/
enum class FnFlags1 : std::uint32_t
{
    isNodiscard             = 0x00000001,

    nodiscardSpelling       = 0x00000002 +
                              0x00000004 +
                              0x00000008 +
                              0x00000010,

    isExplicit              = 0x00000020
};

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo : SymbolInfo
{
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

    Bits<FnFlags0> specs0;
    Bits<FnFlags1> specs1;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_function;

    explicit
    FunctionInfo(
        SymbolID id_ = SymbolID())
        : SymbolInfo(InfoType::IT_function, id_)
    {
    }
};

//------------------------------------------------

} // mrdox
} // clang

#endif
