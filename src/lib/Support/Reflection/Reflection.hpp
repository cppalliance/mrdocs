//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_REFLECTION_REFLECTION_HPP
#define MRDOCS_LIB_SUPPORT_REFLECTION_REFLECTION_HPP

#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ImageInline.hpp>
#include <mrdocs/Metadata/Symbol/Concept.hpp>
#include <mrdocs/Metadata/Symbol/Enum.hpp>
#include <mrdocs/Metadata/Symbol/EnumConstant.hpp>
#include <mrdocs/Metadata/Symbol/Friend.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Guide.hpp>
#include <mrdocs/Metadata/Symbol/Location.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/NamespaceAlias.hpp>
#include <mrdocs/Metadata/Symbol/Overloads.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/RecordInterface.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Symbol/Typedef.hpp>
#include <mrdocs/Metadata/Symbol/Using.hpp>
#include <mrdocs/Metadata/Symbol/Variable.hpp>
#include <mrdocs/Metadata/Symbol/ExtractionMode.hpp>
#include <mrdocs/Metadata/Symbol/FileKind.hpp>
#include <mrdocs/Metadata/Symbol/FunctionClass.hpp>
#include <mrdocs/Metadata/Symbol/RecordBase.hpp>
#include <mrdocs/Metadata/Symbol/RecordKeyKind.hpp>
#include <mrdocs/Metadata/Symbol/Using.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/enum.hpp>

namespace mrdocs {

BOOST_DESCRIBE_STRUCT(
    DocComment,
    (),
    (Document, brief, returns, params, tparams,
     exceptions, sees, preconditions, postconditions,
     relates, related)
)

namespace doc {

BOOST_DESCRIBE_STRUCT(
    ImageInline,
    (InlineCommonBase<InlineKind::Image>, InlineContainer),
    (src, alt)
)

}

BOOST_DESCRIBE_STRUCT(
    ConceptSymbol,
    (SymbolCommonBase<SymbolKind::Concept>),
    (Template, Constraint)
)

BOOST_DESCRIBE_STRUCT(
    EnumSymbol,
    (SymbolCommonBase<SymbolKind::Enum>),
    (Scoped, UnderlyingType, Constants)
)

BOOST_DESCRIBE_STRUCT(
    EnumConstantSymbol,
    (SymbolCommonBase<SymbolKind::EnumConstant>),
    (Initializer)
)

BOOST_DESCRIBE_STRUCT(
    FriendInfo,
    (),
    (Type) // `id` intentionally omitted.
)

BOOST_DESCRIBE_STRUCT(
    FunctionSymbol,
    (SymbolCommonBase<SymbolKind::Function>),
    (ReturnType, Params, Template, FuncClass, Noexcept, Requires,
     IsVariadic, IsDefaulted, IsExplicitlyDefaulted, IsDeleted,
     IsDeletedAsWritten, IsNoReturn, HasOverrideAttr, HasTrailingReturn,
     IsNodiscard, IsExplicitObjectMemberFunction, Constexpr,
     OverloadedOperator, StorageClass, IsRecordMethod, IsVirtual,
     IsVirtualAsWritten, IsPure, IsConst, IsVolatile, IsFinal,
     RefQualifier, Explicit, Attributes)
)

BOOST_DESCRIBE_STRUCT(
    GuideSymbol,
    (SymbolCommonBase<SymbolKind::Guide>),
    (Deduced, Template, Params, Explicit)
)

BOOST_DESCRIBE_STRUCT(
    Location,
    (),
    (FullPath, ShortPath, SourcePath, LineNumber, ColumnNumber, Documented)
)

BOOST_DESCRIBE_STRUCT(
    NamespaceTranche,
    (),
    (Namespaces, NamespaceAliases, Typedefs, Records, Enums,
     Functions, Variables, Concepts, Guides, Usings)
)

BOOST_DESCRIBE_STRUCT(
    NamespaceSymbol,
    (SymbolCommonBase<SymbolKind::Namespace>),
    (IsInline, IsAnonymous, UsingDirectives, Members)
)

BOOST_DESCRIBE_STRUCT(
    NamespaceAliasSymbol,
    (SymbolCommonBase<SymbolKind::NamespaceAlias>),
    (AliasedSymbol)
)

BOOST_DESCRIBE_STRUCT(
    OverloadsSymbol,
    (SymbolCommonBase<SymbolKind::Overloads>),
    (FuncClass, OverloadedOperator, Members) // `ReturnType` intentionally omitted.
)

BOOST_DESCRIBE_STRUCT(
    Param,
    (),
    (Type, Name, Default)
)

BOOST_DESCRIBE_STRUCT(
    RecordSymbol,
    (SymbolCommonBase<SymbolKind::Record>),
    (KeyKind, Template, IsTypeDef, IsFinal, IsFinalDestructor,
     Bases, Derived, Interface, Friends)
)

BOOST_DESCRIBE_STRUCT(
    BaseInfo,
    (),
    (Type, Access, IsVirtual)
)

BOOST_DESCRIBE_STRUCT(
    RecordInterface,
    (),
    (Public, Protected, Private)
)

BOOST_DESCRIBE_STRUCT(
    RecordTranche,
    (),
    (NamespaceAliases, Typedefs, Records, Enums, Functions,
     StaticFunctions, Variables, StaticVariables, Concepts, Guides, Usings)
)

BOOST_DESCRIBE_STRUCT(
    Symbol,
    (),
    (Name, Loc, Kind, id, Access,
     Extraction, IsCopyFromInherited, Parent) // `doc` intentionally omitted.
)

BOOST_DESCRIBE_STRUCT(
    TypedefSymbol,
    (SymbolCommonBase<SymbolKind::Typedef>),
    (Type, IsUsing, Template)
)

BOOST_DESCRIBE_STRUCT(
    UsingSymbol,
    (SymbolCommonBase<SymbolKind::Using>),
    (Class, IntroducedName, ShadowDeclarations)
)

BOOST_DESCRIBE_STRUCT(
    VariableSymbol,
    (SymbolCommonBase<SymbolKind::Variable>),
    (Type, Template, Initializer, StorageClass, IsInline,
     IsConstexpr, IsConstinit, IsThreadLocal, Attributes, IsMaybeUnused,
     IsDeprecated, HasNoUniqueAddress, IsRecordField, IsMutable, IsVariant,
     IsBitfield, BitfieldWidth)
)

BOOST_DESCRIBE_ENUM(
    ExtractionMode, Regular, SeeBelow, ImplementationDefined, Dependency)

BOOST_DESCRIBE_ENUM(FileKind, Source, System, Other)

BOOST_DESCRIBE_ENUM(FunctionClass, Normal, Constructor, Conversion, Destructor)

BOOST_DESCRIBE_ENUM(RecordKeyKind, Struct, Class, Union)

BOOST_DESCRIBE_ENUM(UsingClass, Normal, Typename, Enum)

}

#endif
