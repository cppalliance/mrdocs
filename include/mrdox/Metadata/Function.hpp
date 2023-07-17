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
#include <mrdox/Metadata/Source.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Dom.hpp>
#include <memory>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

/** Return the name of an operator as a string.

    @param include_keyword Whether the name
    should be prefixed with the `operator` keyword.
*/
MRDOX_DECL
std::string_view
getOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Return the short name of an operator as a string.
*/
MRDOX_DECL
std::string_view
getShortOperatorName(
    OperatorKind kind) noexcept;

/** Return the safe name of an operator as a string.

    @param include_keyword Whether the name
    should be prefixed with `operator_`.
*/
MRDOX_DECL
std::string_view
getSafeOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Function classifications */
enum class FunctionClass
{
    Normal = 0,
    Constructor,
    // conversion function
    Conversion,
    Destructor,
    // deduction guide
    Deduction,
};

MRDOX_DECL dom::String toString(FunctionClass kind) noexcept;

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

    BitField<14, 2, ConstexprKind> constexprKind;
    BitField<16, 4, NoexceptKind> exceptionSpec;
    BitField<20, 6, OperatorKind> overloadedOperator;
    BitField<26, 3, StorageClassKind> storageClass;
    BitField<29, 2, ReferenceKind> refQualifier;
};

/** Bit field used with function specifiers.
*/
union FnFlags1
{
    BitFieldFullValue raw;

    BitFlag<0> isNodiscard;

    BitField<1, 3, ExplicitKind> explicitSpec;
};

// KRYSTIAN TODO: attributes (nodiscard, deprecated, and carries_dependency)
// KRYSTIAN TODO: flag to indicate whether this is a function parameter pack
/** Represents a single function parameter */
struct Param
{
    /** The type of this parameter */
    std::unique_ptr<TypeInfo> Type;

    /** The parameter name.

        Unnamed parameters are represented by empty strings.
    */
    std::string Name;

    /** The default argument for this parameter, if any */
    std::string Default;

    Param() = default;

    Param(
        std::unique_ptr<TypeInfo>&& type,
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
    : IsInfo<InfoKind::Function>
    , SourceInfo
{
    std::unique_ptr<TypeInfo> ReturnType; // Info about the return type of this function.
    std::vector<Param> Params; // List of parameters.

    // When present, this function is a template or specialization.
    std::unique_ptr<TemplateInfo> Template;

    // the class of function this is
    FunctionClass Class = FunctionClass::Normal;

    FnFlags0 specs0{.raw{0}};
    FnFlags1 specs1{.raw{0}};

    //--------------------------------------------

    explicit
    FunctionInfo(
        SymbolID ID = SymbolID::zero)
        : IsInfo(ID)
    {
    }
};

//------------------------------------------------

} // mrdox
} // clang

#endif
