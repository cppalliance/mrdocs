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

#include "BitcodeWriter.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/Debug.hpp>
#include <mrdox/Metadata.hpp>
#include <llvm/ADT/IndexedMap.h>
#include <initializer_list>

namespace clang {
namespace mrdox {

//------------------------------------------------

// Since id enums are not zero-indexed, we need to transform the given id into
// its associated index.
struct BlockIdToIndexFunctor
{
    using argument_type = unsigned;
    unsigned operator()(unsigned ID) const
    {
        return ID - BI_FIRST;
    }
};

struct RecordIdToIndexFunctor
{
    using argument_type = unsigned;
    unsigned operator()(unsigned ID) const 
    {
        return ID - RI_FIRST;
    }
};

//------------------------------------------------
//
// Abbrev
//
//------------------------------------------------

using AbbrevDsc = void (*)(std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev);

static void AbbrevGen(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev,
    std::initializer_list<llvm::BitCodeAbbrevOp> Ops)
{
    for (const auto& Op : Ops)
        Abbrev->Add(Op);
}

static void Integer32Abbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. 32-bit signed or unsigned integer
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 32) });
}

static void Integer32ArrayAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. VBR integer (number of 32-bit integers)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 2),
        // 1. Fixed-size array of 32-bit integers
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Array),
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 32) });
}

#if 0
static void Integer16Abbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. 16-bit signed or unsigned integer
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 16) });
}
#endif

static void BoolAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Boolean
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::BoolSize) });
}

static void SymbolIDAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Fixed-size integer (length of the sha1'd USR)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::USRLengthSize),
        // 1. Fixed-size array of Char6 (USR)
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Array),
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::USRBitLengthSize) });
}

static void SymbolIDsAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. VBR integer (number of IDs)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::VBR, 32),
        // 1. Fixed-size array of 20-byte IDs
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Array),
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 8) });
}

static void StringAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Fixed-size integer (length of the following string)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::StringLengthSize),
        // 1. The string blob
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Blob) });
}

// Assumes that the file will not have more than 65535 lines.
static void LocationAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Fixed-size integer (line number)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::LineNumberSize),
        // 1. Boolean (IsFileInRootDir)
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::BoolSize),
        // 2. Fixed-size integer (length of the following string (filename))
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::StringLengthSize),
        // 3. The string blob
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Blob) });
}

struct RecordIdDsc
{
    llvm::StringRef Name;
    AbbrevDsc Abbrev = nullptr;

    RecordIdDsc() = default;
    RecordIdDsc(llvm::StringRef Name, AbbrevDsc Abbrev)
        : Name(Name), Abbrev(Abbrev)
    {
    }

    // Is this 'description' valid?
    operator bool() const
    {
        return Abbrev != nullptr && Name.data() != nullptr && !Name.empty();
    }
};

static
llvm::IndexedMap<llvm::StringRef, BlockIdToIndexFunctor> const
BlockIdNameMap = []()
{
    llvm::IndexedMap<llvm::StringRef, BlockIdToIndexFunctor> BlockIdNameMap;
    BlockIdNameMap.resize(BlockIdCount);

    // There is no init-list constructor for the IndexedMap, so have to
    // improvise
    static const std::vector<std::pair<BlockId, const char* const>> Inits = {
        {BI_VERSION_BLOCK_ID, "VersionBlock"},
        {BI_NAMESPACE_BLOCK_ID, "NamespaceBlock"},
        {BI_ENUM_BLOCK_ID, "EnumBlock"},
        {BI_ENUM_VALUE_BLOCK_ID, "EnumValueBlock"},
        {BI_TYPEDEF_BLOCK_ID, "TypedefBlock"},
        {BI_TYPE_BLOCK_ID, "TypeBlock"},
        {BI_FIELD_TYPE_BLOCK_ID, "FieldTypeBlock"},
        {BI_MEMBER_TYPE_BLOCK_ID, "MemberTypeBlock"},
        {BI_RECORD_BLOCK_ID, "RecordBlock"},
        {BI_BASE_RECORD_BLOCK_ID, "BaseRecordBlock"},
        {BI_FUNCTION_BLOCK_ID, "FunctionBlock"},
        {BI_JAVADOC_BLOCK_ID, "JavadocBlock"},
        {BI_JAVADOC_LIST_BLOCK_ID, "JavadocListBlock"},
        {BI_JAVADOC_NODE_BLOCK_ID, "JavadocNodeBlock"},
        {BI_REFERENCE_BLOCK_ID, "ReferenceBlock"},
        {BI_TEMPLATE_BLOCK_ID, "TemplateBlock"},
        {BI_TEMPLATE_SPECIALIZATION_BLOCK_ID, "TemplateSpecializationBlock"},
        {BI_TEMPLATE_PARAM_BLOCK_ID, "TemplateParamBlock"} };
    Assert(Inits.size() == BlockIdCount);
    for (const auto& Init : Inits)
        BlockIdNameMap[Init.first] = Init.second;
    Assert(BlockIdNameMap.size() == BlockIdCount);
    return BlockIdNameMap;
}();

static
llvm::IndexedMap<RecordIdDsc, RecordIdToIndexFunctor> const
RecordIdNameMap = []()
{
    llvm::IndexedMap<RecordIdDsc, RecordIdToIndexFunctor> RecordIdNameMap;
    RecordIdNameMap.resize(RecordIdCount);

    // There is no init-list constructor for the IndexedMap, so have to
    // improvise
    static const std::vector<std::pair<RecordId, RecordIdDsc>> Inits = {
        {VERSION, {"Version", &Integer32Abbrev}},
        {JAVADOC_LIST_KIND, {"JavadocListKind", &Integer32Abbrev}},
        {JAVADOC_NODE_KIND, {"JavadocNodeKind", &Integer32Abbrev}},
        {JAVADOC_NODE_STRING, {"JavadocNodeString", &StringAbbrev}},
        {JAVADOC_NODE_STYLE, {"JavadocNodeStyle", &Integer32Abbrev}},
        {JAVADOC_NODE_ADMONISH, {"JavadocNodeAdmonish", &Integer32Abbrev}},
        {FIELD_TYPE_NAME, {"Name", &StringAbbrev}},
        {FIELD_DEFAULT_VALUE, {"DefaultValue", &StringAbbrev}},
        {MEMBER_TYPE_NAME, {"Name", &StringAbbrev}},
        {MEMBER_TYPE_ACCESS, {"Access", &Integer32Abbrev}},
        {NAMESPACE_USR, {"USR", &SymbolIDAbbrev}},
        {NAMESPACE_NAME, {"Name", &StringAbbrev}},
        {ENUM_USR, {"USR", &SymbolIDAbbrev}},
        {ENUM_NAME, {"Name", &StringAbbrev}},
        {ENUM_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {ENUM_LOCATION, {"Location", &LocationAbbrev}},
        {ENUM_SCOPED, {"Scoped", &BoolAbbrev}},
        {ENUM_VALUE_NAME, {"Name", &StringAbbrev}},
        {ENUM_VALUE_VALUE, {"Value", &StringAbbrev}},
        {ENUM_VALUE_EXPR, {"Expr", &StringAbbrev}},
        {RECORD_USR, {"USR", &SymbolIDAbbrev}},
        {RECORD_NAME, {"Name", &StringAbbrev}},
        {RECORD_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {RECORD_LOCATION, {"Location", &LocationAbbrev}},
        {RECORD_TAG_TYPE, {"TagType", &Integer32Abbrev}},
        {RECORD_IS_TYPE_DEF, {"IsTypeDef", &BoolAbbrev}},
        {RECORD_FRIENDS, {"Friends", &SymbolIDsAbbrev}},
        {BASE_RECORD_USR, {"USR", &SymbolIDAbbrev}},
        {BASE_RECORD_NAME, {"Name", &StringAbbrev}},
        {BASE_RECORD_TAG_TYPE, {"TagType", &Integer32Abbrev}},
        {BASE_RECORD_IS_VIRTUAL, {"IsVirtual", &BoolAbbrev}},
        {BASE_RECORD_ACCESS, {"Access", &Integer32Abbrev}},
        {BASE_RECORD_IS_PARENT, {"IsParent", &BoolAbbrev}},
        {FUNCTION_USR, {"USR", &SymbolIDAbbrev}},
        {FUNCTION_NAME, {"Name", &StringAbbrev}},
        {FUNCTION_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {FUNCTION_LOCATION, {"Location", &LocationAbbrev}},
        {FUNCTION_ACCESS, {"Access", &Integer32Abbrev}},
        {FUNCTION_IS_METHOD, {"IsMethod", &BoolAbbrev}},
        {FUNCTION_BITS, {"Specs", &Integer32ArrayAbbrev}},
        {REFERENCE_USR, {"USR", &SymbolIDAbbrev}},
        {REFERENCE_NAME, {"Name", &StringAbbrev}},
        {REFERENCE_TYPE, {"RefType", &Integer32Abbrev}},
        {REFERENCE_FIELD, {"Field", &Integer32Abbrev}},
        {TEMPLATE_PARAM_CONTENTS, {"Contents", &StringAbbrev}},
        {TEMPLATE_SPECIALIZATION_OF, {"SpecializationOf", &SymbolIDAbbrev}},
        {TYPEDEF_USR, {"USR", &SymbolIDAbbrev}},
        {TYPEDEF_NAME, {"Name", &StringAbbrev}},
        {TYPEDEF_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {TYPEDEF_IS_USING, {"IsUsing", &BoolAbbrev}} };
    Assert(Inits.size() == RecordIdCount);
    for (const auto& Init : Inits)
    {
        RecordIdNameMap[Init.first] = Init.second;
        Assert((Init.second.Name.size() + 1) <= BitCodeConstants::RecordSize);
    }
    Assert(RecordIdNameMap.size() == RecordIdCount);
    return RecordIdNameMap;
}();

//------------------------------------------------

static
std::vector<std::pair<BlockId, std::vector<RecordId>>> const
RecordsByBlock{
    // Version Block
    {BI_VERSION_BLOCK_ID, {VERSION}},
    // BaseRecordInfo
    {BI_BASE_RECORD_BLOCK_ID,
        {BASE_RECORD_USR, BASE_RECORD_NAME, BASE_RECORD_TAG_TYPE,
        BASE_RECORD_IS_VIRTUAL, BASE_RECORD_ACCESS, BASE_RECORD_IS_PARENT}},
    // EnumInfo
    {BI_ENUM_BLOCK_ID,
        {ENUM_USR, ENUM_NAME, ENUM_DEFLOCATION, ENUM_LOCATION, ENUM_SCOPED}},
    // EnumValue
    {BI_ENUM_VALUE_BLOCK_ID,
        {ENUM_VALUE_NAME, ENUM_VALUE_VALUE, ENUM_VALUE_EXPR}},
    // FieldTypeInfo
    {BI_FIELD_TYPE_BLOCK_ID, {FIELD_TYPE_NAME, FIELD_DEFAULT_VALUE}},
    // FunctionInfo
    {BI_FUNCTION_BLOCK_ID,
        {FUNCTION_USR, FUNCTION_NAME, FUNCTION_DEFLOCATION, FUNCTION_LOCATION,
        FUNCTION_ACCESS, FUNCTION_IS_METHOD, FUNCTION_BITS}},
    // Javadoc
    {BI_JAVADOC_BLOCK_ID,
        {}},
    // List<Javadoc::Node>
    {BI_JAVADOC_LIST_BLOCK_ID,
        {JAVADOC_LIST_KIND}},
    // Javadoc::Node
    {BI_JAVADOC_NODE_BLOCK_ID,
        {JAVADOC_NODE_KIND, JAVADOC_NODE_STRING, JAVADOC_NODE_STYLE,
         JAVADOC_NODE_ADMONISH}},
    // MemberTypeInfo
    {BI_MEMBER_TYPE_BLOCK_ID, {MEMBER_TYPE_NAME, MEMBER_TYPE_ACCESS}},
    // NamespaceInfo
    {BI_NAMESPACE_BLOCK_ID,
        {NAMESPACE_USR, NAMESPACE_NAME}},
    // RecordInfo
    {BI_RECORD_BLOCK_ID,
        {RECORD_USR, RECORD_NAME, RECORD_DEFLOCATION,
        RECORD_LOCATION, RECORD_TAG_TYPE, RECORD_IS_TYPE_DEF,
        RECORD_FRIENDS}},
    // std::vector<Reference>
    {BI_REFERENCE_BLOCK_ID,
        {REFERENCE_USR, REFERENCE_NAME, REFERENCE_TYPE, REFERENCE_FIELD}},
    // TemplateInfo.
    {BI_TEMPLATE_BLOCK_ID, {}},
    {BI_TEMPLATE_PARAM_BLOCK_ID, {TEMPLATE_PARAM_CONTENTS}},
    {BI_TEMPLATE_SPECIALIZATION_BLOCK_ID, {TEMPLATE_SPECIALIZATION_OF}},
    // TypeInfo
    {BI_TYPE_BLOCK_ID, {}},
    // TypedefInfo
    {BI_TYPEDEF_BLOCK_ID,
        {TYPEDEF_USR, TYPEDEF_NAME, TYPEDEF_DEFLOCATION, TYPEDEF_IS_USING}}
};

//------------------------------------------------

BitcodeWriter::
BitcodeWriter(
    llvm::BitstreamWriter &Stream)
    : Stream(Stream)
{
    emitHeader();
    emitBlockInfoBlock();
    emitVersionBlock();
}

bool
BitcodeWriter::
dispatchInfoForWrite(Info const* I)
{
    switch (I->IT)
    {
    case InfoType::IT_namespace:
        emitBlock(*static_cast<NamespaceInfo const*>(I));
        break;
    case InfoType::IT_record:
        emitBlock(*static_cast<RecordInfo const*>(I));
        break;
    case InfoType::IT_function:
        emitBlock(*static_cast<FunctionInfo const*>(I));
        break;
    case InfoType::IT_enum:
        emitBlock(*static_cast<EnumInfo const*>(I));
        break;
    case InfoType::IT_typedef:
        emitBlock(*static_cast<TypedefInfo const*>(I));
        break;
    default:
        llvm::errs() << "Unexpected info, unable to write.\n";
        return true;
    }
    return false;
}

//------------------------------------------------

// Validation and Overview Blocks

/// Emits the magic number header to check
/// that its the right format, in this case, 'DOCS'.
void
BitcodeWriter::
emitHeader()
{
    for (char C : BitCodeConstants::Signature)
        Stream.Emit((unsigned)C, BitCodeConstants::SignatureBitSize);
}

void
BitcodeWriter::
emitBlockInfoBlock()
{
    Stream.EnterBlockInfoBlock();
    for (const auto& Block : RecordsByBlock)
    {
        Assert(Block.second.size() < (1U << BitCodeConstants::SubblockIDSize));
        emitBlockInfo(Block.first, Block.second);
    }
    Stream.ExitBlock();
}

void
BitcodeWriter::
emitVersionBlock()
{
    StreamSubBlockGuard Block(Stream, BI_VERSION_BLOCK_ID);
    emitRecord(VersionNumber, VERSION);
}

//------------------------------------------------

// Block emission

void
BitcodeWriter::
emitBlock(
    NamespaceInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_NAMESPACE_BLOCK_ID);
    emitRecord(I.id, NAMESPACE_USR);
    emitRecord(I.Name, NAMESPACE_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    if(I.javadoc)
        emitBlock(*I.javadoc);
    for (const auto& C : I.Children.Namespaces)
        emitBlock(C, FieldId::F_child_namespace);
    for (const auto& C : I.Children.Records)
        emitBlock(C, FieldId::F_child_record);
    for (auto const& C : I.Children.Functions)
        emitBlock(C, FieldId::F_child_function);
    for (const auto& C : I.Children.Enums)
        emitBlock(C);
    for (const auto& C : I.Children.Typedefs)
        emitBlock(C);
}

void
BitcodeWriter::
emitBlock(
    RecordInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_RECORD_BLOCK_ID);
    emitRecord(I.id, RECORD_USR);
    emitRecord(I.Name, RECORD_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    if(I.javadoc)
        emitBlock(*I.javadoc);
    if (I.DefLoc)
        emitRecord(*I.DefLoc, RECORD_DEFLOCATION);
    for (const auto& L : I.Loc)
        emitRecord(L, RECORD_LOCATION);
    emitRecord(I.TagType, RECORD_TAG_TYPE);
    emitRecord(I.IsTypeDef, RECORD_IS_TYPE_DEF);
    for (const auto& N : I.Members)
        emitBlock(N);
    for (const auto& P : I.Parents)
        emitBlock(P, FieldId::F_parent);
    for (const auto& P : I.VirtualParents)
        emitBlock(P, FieldId::F_vparent);
    for (const auto& PB : I.Bases)
        emitBlock(PB);
    for (const auto& C : I.Children.Records)
        emitBlock(C, FieldId::F_child_record);
    for (auto const& C : I.Children.Functions)
        emitBlock(C, FieldId::F_child_function);
    for (const auto& C : I.Children.Enums)
        emitBlock(C);
    for (const auto& C : I.Children.Typedefs)
        emitBlock(C);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord(I.Friends, RECORD_FRIENDS);
}

void
BitcodeWriter::
emitBlock(
    BaseRecordInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_BASE_RECORD_BLOCK_ID);
    emitRecord(I.id, BASE_RECORD_USR);
    emitRecord(I.Name, BASE_RECORD_NAME);
    emitRecord(I.TagType, BASE_RECORD_TAG_TYPE);
    emitRecord(I.IsVirtual, BASE_RECORD_IS_VIRTUAL);
    emitRecord(I.Access, BASE_RECORD_ACCESS);
    emitRecord(I.IsParent, BASE_RECORD_IS_PARENT);
    for (const auto& M : I.Members)
        emitBlock(M);
}

void
BitcodeWriter::
emitBlock(
    FunctionInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_FUNCTION_BLOCK_ID);
    emitRecord(I.id, FUNCTION_USR);
    emitRecord(I.Name, FUNCTION_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    if(I.javadoc)
        emitBlock(*I.javadoc);
    emitRecord(I.Access, FUNCTION_ACCESS);
    emitRecord(I.IsMethod, FUNCTION_IS_METHOD);
    {
        std::uint32_t v[1] = {
            I.specs.value() };
        emitRecord(v, 1, FUNCTION_BITS);
    }
    if (I.DefLoc)
        emitRecord(*I.DefLoc, FUNCTION_DEFLOCATION);
    for (const auto& L : I.Loc)
        emitRecord(L, FUNCTION_LOCATION);
    emitBlock(I.Parent, FieldId::F_parent);
    emitBlock(I.ReturnType);
    for (const auto& N : I.Params)
        emitBlock(N);
    if (I.Template)
        emitBlock(*I.Template);
}

void
BitcodeWriter::
emitBlock(
    EnumInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_ENUM_BLOCK_ID);
    emitRecord(I.id, ENUM_USR);
    emitRecord(I.Name, ENUM_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    if(I.javadoc)
        emitBlock(*I.javadoc);
    if (I.DefLoc)
        emitRecord(*I.DefLoc, ENUM_DEFLOCATION);
    for (const auto& L : I.Loc)
        emitRecord(L, ENUM_LOCATION);
    emitRecord(I.Scoped, ENUM_SCOPED);
    if (I.BaseType)
        emitBlock(*I.BaseType);
    for (const auto& N : I.Members)
        emitBlock(N);
}

void
BitcodeWriter::
emitBlock(
    EnumValueInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_ENUM_VALUE_BLOCK_ID);
    emitRecord(I.Name, ENUM_VALUE_NAME);
    emitRecord(I.Value, ENUM_VALUE_VALUE);
    emitRecord(I.ValueExpr, ENUM_VALUE_EXPR);
}

void
BitcodeWriter::
emitBlock(
    TypeInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_TYPE_BLOCK_ID);
    emitBlock(T.Type, FieldId::F_type);
}

void
BitcodeWriter::
emitBlock(
    TypedefInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_TYPEDEF_BLOCK_ID);
    emitRecord(T.id, TYPEDEF_USR);
    emitRecord(T.Name, TYPEDEF_NAME);
    for (const auto& N : T.Namespace)
        emitBlock(N, FieldId::F_namespace);
    if(T.javadoc)
        emitBlock(*T.javadoc);
    if (T.DefLoc)
        emitRecord(*T.DefLoc, TYPEDEF_DEFLOCATION);
    emitRecord(T.IsUsing, TYPEDEF_IS_USING);
    emitBlock(T.Underlying);
}

void
BitcodeWriter::
emitBlock(
    FieldTypeInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_FIELD_TYPE_BLOCK_ID);
    emitBlock(T.Type, FieldId::F_type);
    emitRecord(T.Name, FIELD_TYPE_NAME);
    emitRecord(T.DefaultValue, FIELD_DEFAULT_VALUE);
}

void
BitcodeWriter::
emitBlock(
    MemberTypeInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_MEMBER_TYPE_BLOCK_ID);
    emitBlock(T.Type, FieldId::F_type);
    emitRecord(T.Name, MEMBER_TYPE_NAME);
    emitRecord(T.Access, MEMBER_TYPE_ACCESS);
    if(T.javadoc)
        emitBlock(*T.javadoc);
}

void
BitcodeWriter::
emitBlock(
    Javadoc const& jd)
{
    // If the optional<Javadoc> has a value then we
    // always want to emit it, even if it is empty.
    StreamSubBlockGuard Block(Stream, BI_JAVADOC_BLOCK_ID);
    emitBlock(jd.getBlocks());
}

template<class T>
void
BitcodeWriter::
emitBlock(
    List<T> const& list)
{
    StreamSubBlockGuard Block(Stream, BI_JAVADOC_LIST_BLOCK_ID);
    emitRecord(T::static_kind, JAVADOC_LIST_KIND);
    for(Javadoc::Node const& node : list)
        emitBlock(node);
}

void
BitcodeWriter::
emitBlock(
    Javadoc::Node const& I)
{
    StreamSubBlockGuard Block(Stream, BI_JAVADOC_NODE_BLOCK_ID);
    emitRecord(I.kind, JAVADOC_NODE_KIND);
    switch(I.kind)
    {
    case Javadoc::Kind::text:
    {
        auto const& J = static_cast<Javadoc::Text const&>(I);
        emitRecord(J.string, JAVADOC_NODE_STRING);
        break;
    }
    case Javadoc::Kind::styled:
    {
        auto const& J = static_cast<Javadoc::StyledText const&>(I);
        emitRecord(J.style, JAVADOC_NODE_STYLE);
        emitRecord(J.string, JAVADOC_NODE_STRING);
        break;
    }
    case Javadoc::Kind::paragraph:
    {
        auto const& J = static_cast<Javadoc::Paragraph const&>(I);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::brief:
    {
        auto const& J = static_cast<Javadoc::Brief const&>(I);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::admonition:
    {
        auto const& J = static_cast<Javadoc::Admonition const&>(I);
        emitRecord(J.style, JAVADOC_NODE_ADMONISH);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::code:
    {
        auto const& J = static_cast<Javadoc::Code const&>(I);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::returns:
    {
        auto const& J = static_cast<Javadoc::Returns const&>(I);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::param:
    {
        auto const& J = static_cast<Javadoc::Param const&>(I);
        emitRecord(J.name, JAVADOC_NODE_STRING);
        emitBlock(J.children);
        break;
    }
    case Javadoc::Kind::tparam:
    {
        auto const& J = static_cast<Javadoc::TParam const&>(I);
        emitRecord(J.name, JAVADOC_NODE_STRING);
        emitBlock(J.children);
        break;
    }
    default:
        llvm_unreachable("unknown kind");
        break;
    }
}

// AbbreviationMap

constexpr unsigned char BitCodeConstants::Signature[];

void
BitcodeWriter::
AbbreviationMap::
add(RecordId RID,
    unsigned AbbrevID)
{
    Assert(RecordIdNameMap[RID] && "Unknown RecordId.");
    Assert((Abbrevs.find(RID) == Abbrevs.end()) && "Abbreviation already added.");
    Abbrevs[RID] = AbbrevID;
}

unsigned
BitcodeWriter::
AbbreviationMap::
get(RecordId RID) const
{
    Assert(RecordIdNameMap[RID] && "Unknown RecordId.");
    Assert((Abbrevs.find(RID) != Abbrevs.end()) && "Unknown abbreviation.");
    return Abbrevs.lookup(RID);
}

/// Emits a block ID and the block name to the BLOCKINFO block.
void
BitcodeWriter::
emitBlockID(BlockId BID)
{
    const auto& BlockIdName = BlockIdNameMap[BID];
    Assert(BlockIdName.data() && BlockIdName.size() && "Unknown BlockId.");

    Record.clear();
    Record.push_back(BID);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETBID, Record);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_BLOCKNAME,
        ArrayRef<unsigned char>(BlockIdName.bytes_begin(),
            BlockIdName.bytes_end()));
}

/// Emits a record name to the BLOCKINFO block.
void
BitcodeWriter::
emitRecordID(RecordId ID)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    prepRecordData(ID);
    Record.append(RecordIdNameMap[ID].Name.begin(),
        RecordIdNameMap[ID].Name.end());
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETRECORDNAME, Record);
}

// Abbreviations

void
BitcodeWriter::
emitAbbrev(
    RecordId ID, BlockId Block)
{
    Assert(RecordIdNameMap[ID] && "Unknown abbreviation.");
    auto Abbrev = std::make_shared<llvm::BitCodeAbbrev>();
    Abbrev->Add(llvm::BitCodeAbbrevOp(ID));
    RecordIdNameMap[ID].Abbrev(Abbrev);
    Abbrevs.add(ID, Stream.EmitBlockInfoAbbrev(Block, std::move(Abbrev)));
}

//------------------------------------------------
//
// Records
//
//------------------------------------------------

// Integer
template<class Integer>
requires std::is_integral_v<Integer>
void
BitcodeWriter::
emitRecord(
    Integer Value, RecordId ID)
{
    Assert(RecordIdNameMap[ID]);
    if constexpr(sizeof(Integer) == 4)
    {
        Assert(RecordIdNameMap[ID].Abbrev == &Integer32Abbrev); 
    }
#if 0
    else if constexpr(sizeof(Integer) == 2)
    {
        Assert(RecordIdNameMap[ID].Abbrev == &Integer16Abbrev);
    }
#endif
    else
    {
        static_error("can't use Integer type", Value);
    }
    if (!prepRecordData(ID, Value))
        return;
    Record.push_back(static_cast<RecordValue>(Value));
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

// enum
template<class Enum>
requires std::is_enum_v<Enum>
void
BitcodeWriter::
emitRecord(
    Enum Value, RecordId ID)
{
    emitRecord(static_cast<std::underlying_type_t<Enum>>(Value), ID);
}

// Bits
void
BitcodeWriter::
emitRecord(
    std::uint32_t const* p,
    std::size_t n,
    RecordId ID)
{
    Assert(RecordIdNameMap[ID]);
    Assert(RecordIdNameMap[ID].Abbrev ==
        &Integer32ArrayAbbrev);
    bool empty = true;
    for(std::size_t i = 0; i < n; ++i)
    {
        if(p[i] != 0)
        {
            empty = false;
            break;
        }
    }
    if (!prepRecordData(ID, ! empty))
        return;
    Record.push_back(n);
    for(std::size_t i = 0; i < n; ++i)
        Record.push_back(static_cast<RecordValue>(p[i]));
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

// SymbolIDs
void
BitcodeWriter::
emitRecord(
    llvm::SmallVectorImpl<SymbolID> const& Values, RecordId ID)
{
    Assert(RecordIdNameMap[ID]);
    Assert(RecordIdNameMap[ID].Abbrev == &SymbolIDsAbbrev);
    if (!prepRecordData(ID, Values.begin() != Values.end()))
        return;
    Record.push_back(Values.size());
    for(auto const& Sym : Values)
        Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

// SymbolID
void
BitcodeWriter::
emitRecord(
    SymbolID const& Sym,
    RecordId ID)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    Assert(RecordIdNameMap[ID].Abbrev == &SymbolIDAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, Sym != EmptySID))
        return;
    Assert(Sym.size() == 20);
    Record.push_back(Sym.size());
    Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    llvm::StringRef Str, RecordId ID)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    Assert(RecordIdNameMap[ID].Abbrev == &StringAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, !Str.empty()))
        return;
    Assert(Str.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Str.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Str);
}

void
BitcodeWriter::
emitRecord(
    Location const& Loc, RecordId ID)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    Assert(RecordIdNameMap[ID].Abbrev == &LocationAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, true))
        return;
    // FIXME: Assert that the line number
    // is of the appropriate size.
    Record.push_back(Loc.LineNumber);
    Assert(Loc.Filename.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Loc.IsFileInRootDir);
    Record.push_back(Loc.Filename.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Loc.Filename);
}

void
BitcodeWriter::
emitRecord(
    bool Val, RecordId ID)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    Assert(RecordIdNameMap[ID].Abbrev == &BoolAbbrev && "Abbrev type mismatch.");
    if (!prepRecordData(ID, Val))
        return;
    Record.push_back(Val);
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    TemplateInfo const& Templ)
{
    // VFALCO What's going on here? Missing code?
}

bool
BitcodeWriter::
prepRecordData(
    RecordId ID, bool ShouldEmit)
{
    Assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    if (!ShouldEmit)
        return false;
    Record.clear();
    Record.push_back(ID);
    return true;
}

//------------------------------------------------

void
BitcodeWriter::
emitBlockInfo(
    BlockId BID,
    std::vector<RecordId> const& RIDs)
{
    Assert(RIDs.size() < (1U << BitCodeConstants::SubblockIDSize));
    emitBlockID(BID);
    for (RecordId RID : RIDs)
    {
        emitRecordID(RID);
        emitAbbrev(RID, BID);
    }
}

void
BitcodeWriter::
emitBlock(
    Reference const& R, FieldId Field)
{
    if (R.id == globalNamespaceID && R.Name.empty())
        return;
    StreamSubBlockGuard Block(Stream, BI_REFERENCE_BLOCK_ID);
    emitRecord(R.id, REFERENCE_USR);
    emitRecord(R.Name, REFERENCE_NAME);
    emitRecord((unsigned)R.RefType, REFERENCE_TYPE);
    emitRecord((unsigned)Field, REFERENCE_FIELD);
}

void
BitcodeWriter::
emitBlock(
    TemplateInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_BLOCK_ID);
    for (const auto& P : T.Params)
        emitBlock(P);
    if (T.Specialization)
        emitBlock(*T.Specialization);
}

void
BitcodeWriter::
emitBlock(
    TemplateSpecializationInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_SPECIALIZATION_BLOCK_ID);
    emitRecord(T.SpecializationOf, TEMPLATE_SPECIALIZATION_OF);
    for (const auto& P : T.Params)
        emitBlock(P);
}

void
BitcodeWriter::
emitBlock(
    TemplateParamInfo const& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_PARAM_BLOCK_ID);
    emitRecord(T.Contents, TEMPLATE_PARAM_CONTENTS);
}

//------------------------------------------------

/** Write an Info variant to the buffer as bitcode.
*/
Bitcode
writeBitcode(
    Info const& I)
{
    SmallString<0> Buffer;
    llvm::BitstreamWriter Stream(Buffer);
    BitcodeWriter writer(Stream);
    writer.dispatchInfoForWrite(&I);
    return Bitcode{ I.id, std::move(Buffer) };
}

} // mrdox
} // clang
