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
#include <mrdox/Metadata/BitField.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <clang/AST/Attr.h>
#include <vector>

namespace clang {
namespace mrdox {

/** Bit constants used with function specifiers.
*/
union FnFlags0
{
    BitFieldFullValue raw;

    BitFlag<0> isVariadic;
    BitFlag<1> isVirtual;
    BitFlag<2> isVirtualAsWritten;
    BitFlag<3> isPure;
    BitFlag<4> isDefaulted;
    BitFlag<5> isExplicitlyDefaulted;
    BitFlag<6> isDeleted;
    BitFlag<7> isDeletedAsWritten;
    BitFlag<8> isNoReturn;

    BitFlag<9> hasOverrideAttr;
    BitFlag<10> hasTrailingReturn;
    BitField<11, 2, ConstexprSpecKind> constexprKind;
    BitField<13, 4, ExceptionSpecificationType> exceptionSpecType;
    BitField<17, 6, OverloadedOperatorKind> overloadedOperator;
    BitField<23, 3, StorageClass> storageClass;
    BitFlag<26> isConst;
    BitFlag<27> isVolatile;
    BitField<28, 2, RefQualifierKind> refQualifier;
};



/** Bit field used with function specifiers.
*/
union FnFlags1
{
    BitFieldFullValue raw;

    BitFlag<0> isNodiscard;
    BitField<1, 4, WarnUnusedResultAttr::Spelling> nodiscardSpelling;
    BitFlag<5> isExplicit;
    BitField<6, 7> functionKind;
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

    FnFlags0 specs0{.raw{0}};
    FnFlags1 specs1{.raw{0}};

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
