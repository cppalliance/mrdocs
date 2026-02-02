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

#include <mrdocs/Metadata/DocComment/Block/AdmonitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/AdmonitionKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/BriefBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/CodeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/FootnoteDefinitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/HeadingBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/MathBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParamBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParamDirection.hpp>
#include <mrdocs/Metadata/DocComment/Block/PostconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/PreconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/QuoteBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ReturnsBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/SeeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableAlignmentKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableCell.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableRow.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThematicBreakBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThrowsBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TParamBlock.hpp>

#include <mrdocs/Metadata/DocComment/Inline/CodeInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CopyDetailsInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/EmphInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/FootnoteReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/HighlightInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ImageInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineKind.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LineBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LinkInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/MathInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SoftBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrikethroughInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrongInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SubscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SuperscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>

#include <mrdocs/Metadata/Symbol/Concept.hpp>
#include <mrdocs/Metadata/Symbol/Enum.hpp>
#include <mrdocs/Metadata/Symbol/EnumConstant.hpp>
#include <mrdocs/Metadata/Symbol/ExtractionMode.hpp>
#include <mrdocs/Metadata/Symbol/FileKind.hpp>
#include <mrdocs/Metadata/Symbol/Friend.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/FunctionClass.hpp>
#include <mrdocs/Metadata/Symbol/Guide.hpp>
#include <mrdocs/Metadata/Symbol/Location.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/NamespaceAlias.hpp>
#include <mrdocs/Metadata/Symbol/Overloads.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/RecordBase.hpp>
#include <mrdocs/Metadata/Symbol/RecordInterface.hpp>
#include <mrdocs/Metadata/Symbol/RecordKeyKind.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Symbol/Typedef.hpp>
#include <mrdocs/Metadata/Symbol/Using.hpp>
#include <mrdocs/Metadata/Symbol/Variable.hpp>

#include <mrdocs/Metadata/Type/ArrayType.hpp>
#include <mrdocs/Metadata/Type/AutoType.hpp>
#include <mrdocs/Metadata/Type/DecltypeType.hpp>
#include <mrdocs/Metadata/Type/FunctionType.hpp>
#include <mrdocs/Metadata/Type/LValueReferenceType.hpp>
#include <mrdocs/Metadata/Type/NamedType.hpp>
#include <mrdocs/Metadata/Type/PointerType.hpp>
#include <mrdocs/Metadata/Type/RValueReferenceType.hpp>

#include <boost/describe/class.hpp>
#include <boost/describe/enum.hpp>

namespace mrdocs {

/** ValueFrom() support for Optional<DocComment>.
    Required for reflection-based serialization of Symbol::doc.
*/
inline void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<DocComment> const& opt,
    DomCorpus const* domCorpus)
{
    if (opt)
    {
        v = dom::LazyObject(*opt, domCorpus);
    }
    else
    {
        v = nullptr;
    }
}

BOOST_DESCRIBE_STRUCT(
    DocComment,
    (),
    (Document, brief, returns, params, tparams,
     exceptions, sees, preconditions, postconditions,
     relates, related)
)

namespace doc {

// ============================================================================
// Block base types
// ============================================================================

BOOST_DESCRIBE_STRUCT(
    Block,
    (),
    (Kind)
)

BOOST_DESCRIBE_STRUCT(
    BlockContainer,
    (),
    (blocks)
)

// ============================================================================
// Block concrete types (from BlockNodes.inc + BlockCommandNodes.inc)
// ============================================================================

// Markup Blocks
BOOST_DESCRIBE_STRUCT(
    AdmonitionBlock,
    (Block, BlockContainer),
    (admonish)
)

BOOST_DESCRIBE_STRUCT(
    BriefBlock,
    (Block, InlineContainer),
    (copiedFrom)
)

BOOST_DESCRIBE_STRUCT(
    CodeBlock,
    (Block),
    (literal, info)
)

BOOST_DESCRIBE_STRUCT(
    HeadingBlock,
    (Block, InlineContainer),
    (level)
)

BOOST_DESCRIBE_STRUCT(
    ParagraphBlock,
    (Block, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    ListItem,
    (BlockContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    ListBlock,
    (Block),
    (items, listKind)
)

BOOST_DESCRIBE_STRUCT(
    DefinitionListItem,
    (BlockContainer),
    (term)
)

BOOST_DESCRIBE_STRUCT(
    DefinitionListBlock,
    (Block),
    (items)
)

BOOST_DESCRIBE_STRUCT(
    QuoteBlock,
    (Block, BlockContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    ThematicBreakBlock,
    (Block),
    ()
)

BOOST_DESCRIBE_STRUCT(
    FootnoteDefinitionBlock,
    (Block, BlockContainer),
    (label)
)

BOOST_DESCRIBE_STRUCT(
    TableCell,
    (InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    TableRow,
    (),
    (is_header, Cells)
)

BOOST_DESCRIBE_STRUCT(
    TableBlock,
    (Block),
    (Alignments, items)
)

BOOST_DESCRIBE_STRUCT(
    MathBlock,
    (Block),
    (literal)
)

// Metadata Blocks (command blocks)
BOOST_DESCRIBE_STRUCT(
    ParamBlock,
    (Block, InlineContainer),
    (name, direction)
)

BOOST_DESCRIBE_STRUCT(
    PostconditionBlock,
    (Block, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    PreconditionBlock,
    (Block, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    ReturnsBlock,
    (Block, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    SeeBlock,
    (Block, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    ThrowsBlock,
    (Block, InlineContainer),
    (exception)
)

BOOST_DESCRIBE_STRUCT(
    TParamBlock,
    (Block, InlineContainer),
    (name)
)

// ============================================================================
// Inline base types
// ============================================================================

BOOST_DESCRIBE_STRUCT(
    Inline,
    (),
    (Kind)
)

BOOST_DESCRIBE_STRUCT(
    InlineContainer,
    (),
    (children)
)

BOOST_DESCRIBE_STRUCT(
    InlineTextLeaf,
    (),
    (literal)
)

// ============================================================================
// Inline concrete types (from InlineNodes.inc)
// ============================================================================

BOOST_DESCRIBE_STRUCT(
    TextInline,
    (Inline),
    (literal)
)

BOOST_DESCRIBE_STRUCT(
    CodeInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    MathInline,
    (Inline),
    (literal)
)

BOOST_DESCRIBE_STRUCT(
    EmphInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    StrongInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    HighlightInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    StrikethroughInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    SubscriptInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    SuperscriptInline,
    (Inline, InlineContainer),
    ()
)

BOOST_DESCRIBE_STRUCT(
    LinkInline,
    (Inline, InlineContainer),
    (href)
)

BOOST_DESCRIBE_STRUCT(
    ReferenceInline,
    (Inline),
    (literal, id)
)

BOOST_DESCRIBE_STRUCT(
    ImageInline,
    (Inline, InlineContainer),
    (src, alt)
)

BOOST_DESCRIBE_STRUCT(
    FootnoteReferenceInline,
    (Inline),
    (label)
)

BOOST_DESCRIBE_STRUCT(
    CopyDetailsInline,
    (Inline),
    (string, id)
)

BOOST_DESCRIBE_STRUCT(
    LineBreakInline,
    (Inline),
    ()
)

BOOST_DESCRIBE_STRUCT(
    SoftBreakInline,
    (Inline),
    ()
)

// ============================================================================
// Enums for doc types
// ============================================================================

BOOST_DESCRIBE_ENUM(
    InlineKind,
    Reference, CopyDetails, Link, Text, SoftBreak, LineBreak,
    Code, Emph, Strong, Image, FootnoteReference, Strikethrough,
    Math, Superscript, Subscript, Highlight)

BOOST_DESCRIBE_ENUM(
    BlockKind,
    Admonition, Brief, Code, Heading, Paragraph, List,
    DefinitionList, Quote, ThematicBreak, FootnoteDefinition, Table, Math,
    Param, Postcondition, Precondition, Returns, See, Throws, TParam)

}  // namespace doc

// ============================================================================
// Symbol types
// ============================================================================

BOOST_DESCRIBE_STRUCT(
    ConceptSymbol,
    (Symbol),
    (Template, Constraint)
)

BOOST_DESCRIBE_STRUCT(
    EnumSymbol,
    (Symbol),
    (Scoped, UnderlyingType, Constants)
)

BOOST_DESCRIBE_STRUCT(
    EnumConstantSymbol,
    (Symbol),
    (Initializer)
)

BOOST_DESCRIBE_STRUCT(
    FriendInfo,
    (),
    (Type, id)
)

BOOST_DESCRIBE_STRUCT(
    FunctionSymbol,
    (Symbol),
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
    (Symbol),
    (Deduced, Template, Params, Explicit)
)

BOOST_DESCRIBE_STRUCT(
    Location,
    (),
    (ShortPath, SourcePath, LineNumber, ColumnNumber, Documented) // `FullPath` intentionally omitted.
)

BOOST_DESCRIBE_STRUCT(
    NamespaceTranche,
    (),
    (Namespaces, NamespaceAliases, Typedefs, Records, Enums,
     Functions, Variables, Concepts, Guides, Usings)
)

BOOST_DESCRIBE_STRUCT(
    NamespaceSymbol,
    (Symbol),
    (IsInline, IsAnonymous, UsingDirectives, Members)
)

BOOST_DESCRIBE_STRUCT(
    NamespaceAliasSymbol,
    (Symbol),
    (AliasedSymbol)
)

BOOST_DESCRIBE_STRUCT(
    OverloadsSymbol,
    (Symbol),
    (FuncClass, OverloadedOperator, Members, ReturnType)
)

BOOST_DESCRIBE_STRUCT(
    Param,
    (),
    (Type, Name, Default)
)

BOOST_DESCRIBE_STRUCT(
    RecordSymbol,
    (Symbol),
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
    SourceInfo,
    (),
    (DefLoc, Loc)
)

BOOST_DESCRIBE_STRUCT(
    Symbol,
    (),
    (Name, Loc, Kind, id, Access,
     Extraction, IsCopyFromInherited, Parent, doc)
)

BOOST_DESCRIBE_STRUCT(
    TypedefSymbol,
    (Symbol),
    (Type, IsUsing, Template)
)

BOOST_DESCRIBE_STRUCT(
    UsingSymbol,
    (Symbol),
    (Class, IntroducedName, ShadowDeclarations)
)

BOOST_DESCRIBE_STRUCT(
    VariableSymbol,
    (Symbol),
    (Type, Template, Initializer, StorageClass, IsInline,
     IsConstexpr, IsConstinit, IsThreadLocal, Attributes, IsMaybeUnused,
     IsDeprecated, HasNoUniqueAddress, IsRecordField, IsMutable, IsVariant,
     IsBitfield, BitfieldWidth)
)

BOOST_DESCRIBE_STRUCT(
    ArrayType,
    (Type),
    (ElementType, Bounds)
)

BOOST_DESCRIBE_STRUCT(
    AutoType,
    (Type),
    (Keyword, Constraint)
)

BOOST_DESCRIBE_STRUCT(
    DecltypeType,
    (Type),
    (Operand)
)

BOOST_DESCRIBE_STRUCT(
    FunctionType,
    (Type),
    (ReturnType, ParamTypes, RefQualifier, ExceptionSpec, IsVariadic)
)

BOOST_DESCRIBE_STRUCT(
    NamedType,
    (Type),
    (Name, FundamentalType)
)

BOOST_DESCRIBE_STRUCT(
    Type,
    (),
    (Kind, IsPackExpansion, IsConst, IsVolatile, Constraints)
)

BOOST_DESCRIBE_STRUCT(
    LValueReferenceType,
    (Type),
    (PointeeType)
)

BOOST_DESCRIBE_STRUCT(
    PointerType,
    (Type),
    (PointeeType)
)

BOOST_DESCRIBE_STRUCT(
    RValueReferenceType,
    (Type),
    (PointeeType)
)

BOOST_DESCRIBE_STRUCT(
    Name,
    (),
    (Kind, id, Identifier, Prefix)
)

BOOST_DESCRIBE_ENUM(
    ExtractionMode, Regular, SeeBelow, ImplementationDefined, Dependency)

BOOST_DESCRIBE_ENUM(FileKind, Source, System, Other)

BOOST_DESCRIBE_ENUM(FunctionClass, Normal, Constructor, Conversion, Destructor)

BOOST_DESCRIBE_ENUM(RecordKeyKind, Struct, Class, Union)

BOOST_DESCRIBE_ENUM(UsingClass, Normal, Typename, Enum)

namespace doc {

BOOST_DESCRIBE_ENUM(AdmonitionKind, none, note, tip, important, caution, warning)

}

}

#endif
