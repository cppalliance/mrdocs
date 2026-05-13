//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP
#define MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Symbol/FunctionClass.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Metadata for a function or method.
*/
struct FunctionSymbol final
    : SymbolCommonBase<SymbolKind::Function>
{
    /** Info about the return type of this function.

        If the function has a deduced return type, this contains
        `auto` to indicate that.

        By default, we also use `auto` in the member to indicate
        an unknown return type.
    */
    Polymorphic<Type> ReturnType = Polymorphic<Type>(AutoType{});

    /// List of parameters.
    std::vector<Param> Params;

    /// When present, this function is a template or specialization.
    Optional<TemplateInfo> Template;

    /// The class of function this is
    FunctionClass FuncClass = FunctionClass::Normal;

    /** Exception specification for the function.
    */
    NoexceptInfo Noexcept;
    /** Constrained requires-clause if present.
    */
    ExprInfo Requires;
    /** True when the function is variadic.
    */
    bool IsVariadic = false;
    /** True when this declaration is implicitly defaulted.
    */
    bool IsDefaulted = false;
    /** True when explicitly defaulted with `= default`.
    */
    bool IsExplicitlyDefaulted = false;
    /** True when this declaration is deleted.
    */
    bool IsDeleted = false;
    /** True when deleted as written (vs deduced).
    */
    bool IsDeletedAsWritten = false;
    /** True when marked [[noreturn]] or equivalent.
    */
    bool IsNoReturn = false;
    /** True when annotated with override.
    */
    bool HasOverrideAttr = false;
    /** True when using a trailing return type.
    */
    bool HasTrailingReturn = false;
    /** True when declared [[nodiscard]].
    */
    bool IsNodiscard = false;
    /** True when explicit object parameter syntax is used.
    */
    bool IsExplicitObjectMemberFunction = false;
    /** constexpr/consteval specifier.
    */
    ConstexprKind Constexpr = ConstexprKind::None;
    /** Overloaded operator kind, if any.
    */
    OperatorKind OverloadedOperator = OperatorKind::None;
    /** Storage class specifier.
    */
    StorageClassKind StorageClass = StorageClassKind::None;
    /** Collected attributes attached to the declaration.
    */
    std::vector<std::string> Attributes;

    // CXXMethodDecl
    /** True when this is a non-static member function.
    */
    bool IsRecordMethod = false;
    /** True when declared virtual (after overrides).
    */
    bool IsVirtual = false;
    /** True when explicitly written virtual.
    */
    bool IsVirtualAsWritten = false;
    /** True when the function is pure virtual.
    */
    bool IsPure = false;
    /** True when qualified const.
    */
    bool IsConst = false;
    /** True when qualified volatile.
    */
    bool IsVolatile = false;
    /** True when final-qualified.
    */
    bool IsFinal = false;
    /** Reference qualifier on the member function, if any.
    */
    ReferenceKind RefQualifier = ReferenceKind::None;
    /** explicit-specifier information.
    */
    ExplicitInfo Explicit;

    /** Back-reference to the function object implementation type.

        When set, this function was synthesized from a function
        object variable: the function does not participate in ADL
        and taking its address is undefined behavior.
    */
    Optional<SymbolID> FunctionObjectImpl;

    /** Whether this function is listed on its primary's page.

        A presentation-layer flag set by `SpecializationFinalizer`
        when *both* conditions hold:

          1. This function is a template specialization (AST-local,
             equivalent to `Template->specializationKind() != Primary`).
          2. Its primary is being extracted in
             @ref ExtractionMode::Regular (cross-symbol, needs the
             corpus).

        When set, the function is rendered in its primary's
        "Specializations" section and suppressed from the parent
        scope's listing. Orphan specializations (primary excluded
        from extraction) fail condition 2 and keep the flag `false`,
        so they remain reachable from the parent scope. The name
        deliberately encodes the resulting placement rather than
        the AST property in 1, which `Template` already exposes.
    */
    bool IsListedOnPrimary = false;

    /** Specializations whose primary is this function.

        Populated by `SpecializationFinalizer` with the IDs of
        function-template specializations referring to this
        function as their primary. Sorted by referent name
        then ID.
    */
    std::vector<SymbolID> Specializations;

    //--------------------------------------------

    /** Construct a function symbol with its ID.
    */
    explicit FunctionSymbol(SymbolID const& ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare functions by signature, qualifiers, and metadata.
    */
    std::strong_ordering
    operator<=>(FunctionSymbol const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(
    FunctionSymbol,
    (SymbolCommonBase<SymbolKind::Function>),
    (ReturnType, Params, Template, FuncClass, Noexcept, Requires,
     IsVariadic, IsDefaulted, IsExplicitlyDefaulted, IsDeleted,
     IsDeletedAsWritten, IsNoReturn, HasOverrideAttr, HasTrailingReturn,
     IsNodiscard, IsExplicitObjectMemberFunction, Constexpr,
     OverloadedOperator, StorageClass, IsRecordMethod, IsVirtual,
     IsVirtualAsWritten, IsPure, IsConst, IsVolatile, IsFinal,
     RefQualifier, Explicit, Attributes, FunctionObjectImpl,
     IsListedOnPrimary, Specializations)
)

/** Map a vector of parameters to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param params The list of parameters to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::vector<Param> const& params,
    DomCorpus const* domCorpus)
{
    v = dom::LazyArray(params, domCorpus);
}

/** Check whether a function is a default constructor.

    A default constructor is a constructor for which each
    parameter that is not a function parameter pack has a
    default argument (including the case of a constructor
    with no parameters) ([class.default.ctor]).

    @param func The function to check.
    @return Whether @p func is a default constructor.
*/
MRDOCS_DECL
bool
isDefaultConstructor(FunctionSymbol const& func);

/** Check whether a function is a copy constructor.

    A copy constructor is a non-template constructor whose
    first parameter is an lvalue reference to the possibly
    cv-qualified record type, with all remaining parameters
    having defaults ([class.copy.ctor]).

    @param func The function to check.
    @return Whether @p func is a copy constructor.
*/
MRDOCS_DECL
bool
isCopyConstructor(FunctionSymbol const& func);

/** Check whether a function is a move constructor.

    A move constructor is a non-template constructor whose
    first parameter is an rvalue reference to the possibly
    cv-qualified record type, with all remaining parameters
    having defaults ([class.copy.ctor]).

    @param func The function to check.
    @return Whether @p func is a move constructor.
*/
MRDOCS_DECL
bool
isMoveConstructor(FunctionSymbol const& func);

/** Check whether a function is a copy assignment operator.

    A copy assignment operator is a non-template operator=
    whose parameter is of type X, X&, const X&, volatile X&,
    or const volatile X& ([class.copy.assign]).

    @param func The function to check.
    @return Whether @p func is a copy assignment operator.
*/
MRDOCS_DECL
bool
isCopyAssignment(FunctionSymbol const& func);

/** Check whether a function is a move assignment operator.

    A move assignment operator is a non-template operator=
    whose parameter is an rvalue reference to the possibly
    cv-qualified record type ([class.copy.assign]).

    @param func The function to check.
    @return Whether @p func is a move assignment operator.
*/
MRDOCS_DECL
bool
isMoveAssignment(FunctionSymbol const& func);

/** Check whether a function is a special member function.

    A special member function is a default constructor,
    copy/move constructor, copy/move assignment operator,
    or destructor ([special]).

    @param func The function to check.
    @return Whether @p func is a special member function.
*/
MRDOCS_DECL
bool
isSpecialMemberFunction(FunctionSymbol const& func);

/** Determine if one function would override the other

    @param base The base function
    @param derived The derived function
*/
MRDOCS_DECL
bool
overrides(FunctionSymbol const& base, FunctionSymbol const& derived);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_FUNCTION_HPP
