//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_FUNCTION_HPP
#define MRDOX_API_METADATA_FUNCTION_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <mrdox/Metadata/Field.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <clang/AST/Attr.h>
#include <memory>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

/** An enumeration of the different kinds of functions.
*/
enum class FunctionKind
{
    // VFALCO The most frequent function kind should be
    // here, since the bitstream writer does not emit zeroes.
    Plain = 0,

    // The operator kind values have to match the clang enumeration
    OpNew /* = clang::OverloadedOperatorKind::OO_New */,
    OpDelete,
    OpArray_New,
    OpArray_Delete,
    OpPlus,
    OpMinus,
    OpStar,
    OpSlash,
    OpPercent,
    OpCaret,
    OpAmp,
    OpPipe,
    OpTilde,
    OpExclaim,
    OpEqual,
    OpLess,
    OpGreater,
    OpPlusEqual,
    OpMinusEqual,
    OpStarEqual,
    OpSlashEqual,
    OpPercentEqual,
    OpCaretEqual,
    OpAmpEqual,
    OpPipeEqual,
    OpLessLess,
    OpGreaterGreater,
    OpLessLessEqual,
    OpGreaterGreaterEqual,
    OpEqualEqual,
    OpExclaimEqual,
    OpLessEqual,
    OpGreaterEqual,
    OpSpaceship,
    OpAmpAmp,
    OpPipePipe,
    OpPlusPlus,
    OpMinusMinus,
    OpComma,
    OpArrowStar,
    OpArrow,
    OpCall,
    OpSubscript,
    OpConditional,
    OpCoawait,
    NUM_OVERLOADED_OPERATORS /* = clang::OverloadedOperatorKind::NUM_OVERLOADED_OPERATORS */,

    Destructor = NUM_OVERLOADED_OPERATORS,
    Constructor,
    Conversion
};

/** Return the function kind corresponding to clang's enum
*/
FunctionKind
getFunctionKind(
    OverloadedOperatorKind OOK) noexcept;

/** Return a unique string constant for the kind.
*/
std::string_view
getFunctionKindString(
    FunctionKind kind) noexcept;


/** Bit constants used with function specifiers.
*/
union FnFlags0
{
    BitFieldFullValue raw;

    BitFlag < 0> isVariadic;
    BitFlag < 1> isVirtual;
    BitFlag < 2> isVirtualAsWritten;
    BitFlag < 3> isPure;
    BitFlag < 4> isDefaulted;
    BitFlag < 5> isExplicitlyDefaulted;
    BitFlag < 6> isDeleted;
    BitFlag < 7> isDeletedAsWritten;
    BitFlag < 8> isNoReturn;
    BitFlag < 9> hasOverrideAttr;
    BitFlag <10> hasTrailingReturn;
    BitFlag <11> isConst;
    BitFlag <12> isVolatile;
    BitField<13> isFinal;

    BitField<14, 2, ConstexprSpecKind> constexprKind;
    BitField<16, 4, ExceptionSpecificationType> exceptionSpecType;
    BitField<20, 6, OverloadedOperatorKind> overloadedOperator;
    BitField<26, 3, StorageClass> storageClass;
    BitField<29, 2, RefQualifierKind> refQualifier;
};

/** Bit field used with function specifiers.
*/
union FnFlags1
{
    BitFieldFullValue raw;

    BitFlag<0> isNodiscard;
    BitFlag<1> isExplicit;

    BitField<2, 4, WarnUnusedResultAttr::Spelling> nodiscardSpelling;
    BitField<6, 7, FunctionKind> functionKind;
};

// KRYSTIAN TODO: attributes (nodiscard, deprecated, and carries_dependency)
// KRYSTIAN TODO: flag to indicate whether this is a function parameter pack
/** Represents a single function parameter */
struct Param
{
    /** The type of this parameter */
    TypeInfo Type;

    /** The parameter name.

        Unnamed parameters are represented by empty strings.
    */
    std::string Name;

    /** The default argument for this parameter, if any */
    std::string Default;

    Param() = default;

    Param(
        TypeInfo&& type,
        std::string&& name,
        std::string&& def_arg)
        : Type(std::move(type))
        , Name(std::move(name))
        , Default(std::move(def_arg))
    {
    }
};

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo
    : SymbolInfo
{
    friend class ASTVisitor;

    TypeInfo ReturnType;   // Info about the return type of this function.
    std::vector<Param> Params; // List of parameters.

    // When present, this function is a template or specialization.
    std::unique_ptr<TemplateInfo> Template;

    FnFlags0 specs0{.raw{0}};
    FnFlags1 specs1{.raw{0}};

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_function;

    explicit
    FunctionInfo(
        SymbolID ID = SymbolID::zero)
        : SymbolInfo(InfoType::IT_function, ID)
    {
    }

private:
    explicit
    FunctionInfo(
        std::unique_ptr<TemplateInfo>&& T)
        : SymbolInfo(InfoType::IT_function)
        , Template(std::move(T))
    {
    }
};

//------------------------------------------------

} // mrdox
} // clang

#endif
