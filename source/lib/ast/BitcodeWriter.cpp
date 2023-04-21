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
#include "ast/ParseJavadoc.hpp"
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

using AbbrevDsc = void (*)(std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev);

static void AbbrevGen(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev,
    std::initializer_list<llvm::BitCodeAbbrevOp> Ops)
{
    for (const auto& Op : Ops)
        Abbrev->Add(Op);
}

static void BoolAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Boolean
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::BoolSize) });
}

static void IntAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. Fixed-size integer
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::IntSize) });
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
    assert(Inits.size() == BlockIdCount);
    for (const auto& Init : Inits)
        BlockIdNameMap[Init.first] = Init.second;
    assert(BlockIdNameMap.size() == BlockIdCount);
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
        {VERSION, {"Version", &IntAbbrev}},
        {JAVADOC_LIST_KIND, {"JavadocListKind", &IntAbbrev}},
        {JAVADOC_NODE_KIND, {"JavadocNodeKind", &IntAbbrev}},
        {JAVADOC_NODE_STRING, {"JavadocNodeString", &StringAbbrev}},
        {JAVADOC_NODE_STYLE, {"JavadocNodeStyle", &IntAbbrev}},
        {JAVADOC_NODE_ADMONISH, {"JavadocNodeAdmonish", &IntAbbrev}},
        {FIELD_TYPE_NAME, {"Name", &StringAbbrev}},
        {FIELD_DEFAULT_VALUE, {"DefaultValue", &StringAbbrev}},
        {MEMBER_TYPE_NAME, {"Name", &StringAbbrev}},
        {MEMBER_TYPE_ACCESS, {"Access", &IntAbbrev}},
        {NAMESPACE_USR, {"USR", &SymbolIDAbbrev}},
        {NAMESPACE_NAME, {"Name", &StringAbbrev}},
        {NAMESPACE_PATH, {"Path", &StringAbbrev}},
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
        {RECORD_PATH, {"Path", &StringAbbrev}},
        {RECORD_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {RECORD_LOCATION, {"Location", &LocationAbbrev}},
        {RECORD_TAG_TYPE, {"TagType", &IntAbbrev}},
        {RECORD_IS_TYPE_DEF, {"IsTypeDef", &BoolAbbrev}},
        {BASE_RECORD_USR, {"USR", &SymbolIDAbbrev}},
        {BASE_RECORD_NAME, {"Name", &StringAbbrev}},
        {BASE_RECORD_PATH, {"Path", &StringAbbrev}},
        {BASE_RECORD_TAG_TYPE, {"TagType", &IntAbbrev}},
        {BASE_RECORD_IS_VIRTUAL, {"IsVirtual", &BoolAbbrev}},
        {BASE_RECORD_ACCESS, {"Access", &IntAbbrev}},
        {BASE_RECORD_IS_PARENT, {"IsParent", &BoolAbbrev}},
        {FUNCTION_USR, {"USR", &SymbolIDAbbrev}},
        {FUNCTION_NAME, {"Name", &StringAbbrev}},
        {FUNCTION_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {FUNCTION_LOCATION, {"Location", &LocationAbbrev}},
        {FUNCTION_ACCESS, {"Access", &IntAbbrev}},
        {FUNCTION_IS_METHOD, {"IsMethod", &BoolAbbrev}},
        {REFERENCE_USR, {"USR", &SymbolIDAbbrev}},
        {REFERENCE_NAME, {"Name", &StringAbbrev}},
        {REFERENCE_TYPE, {"RefType", &IntAbbrev}},
        {REFERENCE_PATH, {"Path", &StringAbbrev}},
        {REFERENCE_FIELD, {"Field", &IntAbbrev}},
        {TEMPLATE_PARAM_CONTENTS, {"Contents", &StringAbbrev}},
        {TEMPLATE_SPECIALIZATION_OF, {"SpecializationOf", &SymbolIDAbbrev}},
        {TYPEDEF_USR, {"USR", &SymbolIDAbbrev}},
        {TYPEDEF_NAME, {"Name", &StringAbbrev}},
        {TYPEDEF_DEFLOCATION, {"DefLocation", &LocationAbbrev}},
        {TYPEDEF_IS_USING, {"IsUsing", &BoolAbbrev}} };
    assert(Inits.size() == RecordIdCount);
    for (const auto& Init : Inits)
    {
        RecordIdNameMap[Init.first] = Init.second;
        assert((Init.second.Name.size() + 1) <= BitCodeConstants::RecordSize);
    }
    assert(RecordIdNameMap.size() == RecordIdCount);
    return RecordIdNameMap;
}();

//------------------------------------------------

static
std::vector<std::pair<BlockId, std::vector<RecordId>>> const
RecordsByBlock{
    // Version Block
    {BI_VERSION_BLOCK_ID, {VERSION}},
    // Javadoc Block
    {BI_JAVADOC_BLOCK_ID,
        {}},
    // List<Javadoc::Node> Block
    {BI_JAVADOC_LIST_BLOCK_ID,
        {JAVADOC_LIST_KIND}},
    // Javadoc::Node Block
    {BI_JAVADOC_NODE_BLOCK_ID,
        {JAVADOC_NODE_KIND, JAVADOC_NODE_STRING, JAVADOC_NODE_STYLE,
         JAVADOC_NODE_ADMONISH}},
    // Type Block
    {BI_TYPE_BLOCK_ID, {}},
    // <mrdox/FieldType.hpp> Block
    {BI_FIELD_TYPE_BLOCK_ID, {FIELD_TYPE_NAME, FIELD_DEFAULT_VALUE}},
    // MemberType Block
    {BI_MEMBER_TYPE_BLOCK_ID, {MEMBER_TYPE_NAME, MEMBER_TYPE_ACCESS}},
    // Enum Block
    {BI_ENUM_BLOCK_ID,
        {ENUM_USR, ENUM_NAME, ENUM_DEFLOCATION, ENUM_LOCATION, ENUM_SCOPED}},
    // Enum Value Block
    {BI_ENUM_VALUE_BLOCK_ID,
        {ENUM_VALUE_NAME, ENUM_VALUE_VALUE, ENUM_VALUE_EXPR}},
    // Typedef Block
    {BI_TYPEDEF_BLOCK_ID,
        {TYPEDEF_USR, TYPEDEF_NAME, TYPEDEF_DEFLOCATION, TYPEDEF_IS_USING}},
    // Namespace Block
    {BI_NAMESPACE_BLOCK_ID,
        {NAMESPACE_USR, NAMESPACE_NAME, NAMESPACE_PATH}},
    // Record Block
    {BI_RECORD_BLOCK_ID,
        {RECORD_USR, RECORD_NAME, RECORD_PATH, RECORD_DEFLOCATION,
        RECORD_LOCATION, RECORD_TAG_TYPE, RECORD_IS_TYPE_DEF}},
    // BaseRecord Block
    {BI_BASE_RECORD_BLOCK_ID,
        {BASE_RECORD_USR, BASE_RECORD_NAME, BASE_RECORD_PATH,
        BASE_RECORD_TAG_TYPE, BASE_RECORD_IS_VIRTUAL, BASE_RECORD_ACCESS,
        BASE_RECORD_IS_PARENT}},
    // Function Block
    {BI_FUNCTION_BLOCK_ID,
        {FUNCTION_USR, FUNCTION_NAME, FUNCTION_DEFLOCATION, FUNCTION_LOCATION,
        FUNCTION_ACCESS, FUNCTION_IS_METHOD}},
    // Reference Block
    {BI_REFERENCE_BLOCK_ID,
        {REFERENCE_USR, REFERENCE_NAME, REFERENCE_TYPE,
        REFERENCE_PATH, REFERENCE_FIELD}},
    // Template Blocks.
    {BI_TEMPLATE_BLOCK_ID, {}},
        {BI_TEMPLATE_PARAM_BLOCK_ID, {TEMPLATE_PARAM_CONTENTS}},
        {BI_TEMPLATE_SPECIALIZATION_BLOCK_ID, {TEMPLATE_SPECIALIZATION_OF}}
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
        assert(Block.second.size() < (1U << BitCodeConstants::SubblockIDSize));
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
    emitRecord(I.USR, NAMESPACE_USR);
    emitRecord(I.Name, NAMESPACE_NAME);
    emitRecord(I.Path, NAMESPACE_PATH);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    emitBlock(I.javadoc);
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
    emitRecord(I.USR, RECORD_USR);
    emitRecord(I.Name, RECORD_NAME);
    emitRecord(I.Path, RECORD_PATH);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    emitBlock(I.javadoc);
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
}

void
BitcodeWriter::
emitBlock(
    BaseRecordInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_BASE_RECORD_BLOCK_ID);
    emitRecord(I.USR, BASE_RECORD_USR);
    emitRecord(I.Name, BASE_RECORD_NAME);
    emitRecord(I.Path, BASE_RECORD_PATH);
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
    emitRecord(I.USR, FUNCTION_USR);
    emitRecord(I.Name, FUNCTION_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    emitBlock(I.javadoc);
    emitRecord(I.Access, FUNCTION_ACCESS);
    emitRecord(I.IsMethod, FUNCTION_IS_METHOD);
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
    emitRecord(I.USR, ENUM_USR);
    emitRecord(I.Name, ENUM_NAME);
    for (const auto& N : I.Namespace)
        emitBlock(N, FieldId::F_namespace);
    emitBlock(I.javadoc);
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
    emitRecord(T.USR, TYPEDEF_USR);
    emitRecord(T.Name, TYPEDEF_NAME);
    for (const auto& N : T.Namespace)
        emitBlock(N, FieldId::F_namespace);
    emitBlock(T.javadoc);
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
    emitBlock(T.javadoc);
}

void
BitcodeWriter::
emitBlock(
    Javadoc const& jd)
{
    if(! jd.empty())
    {
        StreamSubBlockGuard Block(Stream, BI_JAVADOC_BLOCK_ID);
        emitBlock(jd.getBlocks());
    }
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
    assert(RecordIdNameMap[RID] && "Unknown RecordId.");
    assert((Abbrevs.find(RID) == Abbrevs.end()) && "Abbreviation already added.");
    Abbrevs[RID] = AbbrevID;
}

unsigned
BitcodeWriter::
AbbreviationMap::
get(RecordId RID) const
{
    assert(RecordIdNameMap[RID] && "Unknown RecordId.");
    assert((Abbrevs.find(RID) != Abbrevs.end()) && "Unknown abbreviation.");
    return Abbrevs.lookup(RID);
}

/// Emits a block ID and the block name to the BLOCKINFO block.
void
BitcodeWriter::
emitBlockID(BlockId BID)
{
    const auto& BlockIdName = BlockIdNameMap[BID];
    assert(BlockIdName.data() && BlockIdName.size() && "Unknown BlockId.");

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
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
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
    assert(RecordIdNameMap[ID] && "Unknown abbreviation.");
    auto Abbrev = std::make_shared<llvm::BitCodeAbbrev>();
    Abbrev->Add(llvm::BitCodeAbbrevOp(ID));
    RecordIdNameMap[ID].Abbrev(Abbrev);
    Abbrevs.add(ID, Stream.EmitBlockInfoAbbrev(Block, std::move(Abbrev)));
}

//------------------------------------------------
//
// emitRecord
//
//------------------------------------------------

template<class Enum, class>
void
BitcodeWriter::
emitRecord(
    Enum value, RecordId ID)
{
    emitRecord(static_cast<std::underlying_type_t<Enum>>(value), ID);
}

void
BitcodeWriter::
emitRecord(
    SymbolID const& Sym,
    RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &SymbolIDAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, Sym != EmptySID))
        return;
    assert(Sym.size() == 20);
    Record.push_back(Sym.size());
    Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    llvm::StringRef Str, RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &StringAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, !Str.empty()))
        return;
    assert(Str.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Str.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Str);
}

void
BitcodeWriter::
emitRecord(
    Location const& Loc, RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &LocationAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, true))
        return;
    // FIXME: Assert that the line number
    // is of the appropriate size.
    Record.push_back(Loc.LineNumber);
    assert(Loc.Filename.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Loc.IsFileInRootDir);
    Record.push_back(Loc.Filename.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Loc.Filename);
}

void
BitcodeWriter::
emitRecord(
    bool Val, RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &BoolAbbrev && "Abbrev type mismatch.");
    if (!prepRecordData(ID, Val))
        return;
    Record.push_back(Val);
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    int Val, RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &IntAbbrev && "Abbrev type mismatch.");
    if (!prepRecordData(ID, Val))
        return;
    // FIXME: Assert that the integer is of the appropriate size.
    Record.push_back(Val);
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    unsigned Val, RecordId ID)
{
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    assert(RecordIdNameMap[ID].Abbrev == &IntAbbrev && "Abbrev type mismatch.");
    if (!prepRecordData(ID, Val))
        return;
    assert(Val < (1U << BitCodeConstants::IntSize));
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
    assert(RecordIdNameMap[ID] && "Unknown RecordId.");
    if (!ShouldEmit)
        return false;
    Record.clear();
    Record.push_back(ID);
    return true;
}

void
BitcodeWriter::
emitBlockInfo(
    BlockId BID,
    std::vector<RecordId> const& RIDs)
{
    assert(RIDs.size() < (1U << BitCodeConstants::SubblockIDSize));
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
    if (R.USR == EmptySID && R.Name.empty())
        return;
    StreamSubBlockGuard Block(Stream, BI_REFERENCE_BLOCK_ID);
    emitRecord(R.USR, REFERENCE_USR);
    emitRecord(R.Name, REFERENCE_NAME);
    emitRecord((unsigned)R.RefType, REFERENCE_TYPE);
    emitRecord(R.Path, REFERENCE_PATH);
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

/** Write an Info variant to the bitstream.
*/
void
writeBitcode(
    Info const& I,
    llvm::BitstreamWriter& Stream)
{
    BitcodeWriter writer(Stream);
    writer.dispatchInfoForWrite(&I);
}

} // mrdox
} // clang
