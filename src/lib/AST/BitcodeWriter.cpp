//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Bitcode.hpp"
#include "BitcodeWriter.hpp"
#include <memory>

namespace clang {
namespace mrdocs {

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

struct RecordIDToIndexFunctor
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

static void Integer64Abbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // 0. 64-bit signed or unsigned integer
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 32),
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 32),
    });
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
        // 1. File kind
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 3),
        // 2. Whether this declaration has docs
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 1),
        // 3. Fixed-size integer, length of the path
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::StringLengthSize),
        // 4. Fixed-size integer, length of the path + filename
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::StringLengthSize),
        // 5. The string blob
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Blob) });
}

// Assumes that the file will not have more than 65535 lines.
static void NoexceptAbbrev(
    std::shared_ptr<llvm::BitCodeAbbrev>& Abbrev)
{
    AbbrevGen(Abbrev, {
        // NoexceptInfo::Implicit
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::BoolSize),
        // NoexceptInfo::Kind
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed, 2),
        // NoexceptInfo::Operand
        llvm::BitCodeAbbrevOp(
            llvm::BitCodeAbbrevOp::Fixed,
            BitCodeConstants::StringLengthSize),
        // 5. The string blob
        llvm::BitCodeAbbrevOp(llvm::BitCodeAbbrevOp::Blob) });
}

//------------------------------------------------

struct RecordIDDsc
{
    llvm::StringRef Name;
    AbbrevDsc Abbrev = nullptr;

    RecordIDDsc() = default;
    RecordIDDsc(llvm::StringRef Name, AbbrevDsc Abbrev)
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
    static const std::vector<std::pair<BlockID, const char* const>> Inits = {
        {BI_VERSION_BLOCK_ID, "VersionBlock"},
        {BI_BASE_BLOCK_ID, "BaseBlock"},
        {BI_INFO_PART_ID, "InfoPart"},
        {BI_SOURCE_INFO_ID, "SourceInfoBlock"},
        {BI_SCOPE_INFO_ID, "ScopeInfoBlock"},
        {BI_LOOKUP_INFO_ID, "LookupInfoBlock"},
        {BI_NAMESPACE_BLOCK_ID, "NamespaceBlock"},
        {BI_ENUM_BLOCK_ID, "EnumBlock"},
        {BI_EXPR_BLOCK_ID, "ExprBlock"},
        {BI_BITFIELD_WIDTH_BLOCK_ID, "BitfieldWidthBlock"},
        {BI_TYPEDEF_BLOCK_ID, "TypedefBlock"},
        {BI_TYPEINFO_BLOCK_ID, "TypeInfoBlock"},
        {BI_TYPEINFO_PARENT_BLOCK_ID, "TypeInfoParentBlock"},
        {BI_TYPEINFO_CHILD_BLOCK_ID, "TypeInfoChildBlock"},
        {BI_TYPEINFO_PARAM_BLOCK_ID, "TypeInfoParamBlock"},
        {BI_FIELD_BLOCK_ID, "FieldBlock"},
        {BI_RECORD_BLOCK_ID, "RecordBlock"},
        {BI_FUNCTION_BLOCK_ID, "FunctionBlock"},
        {BI_GUIDE_BLOCK_ID, "GuideBlock"},
        {BI_FUNCTION_PARAM_BLOCK_ID, "FunctionParamBlock"},
        {BI_JAVADOC_BLOCK_ID, "JavadocBlock"},
        {BI_JAVADOC_LIST_BLOCK_ID, "JavadocListBlock"},
        {BI_JAVADOC_NODE_BLOCK_ID, "JavadocNodeBlock"},
        {BI_TEMPLATE_ARG_BLOCK_ID, "TemplateArgBlock"},
        {BI_TEMPLATE_BLOCK_ID, "TemplateBlock"},
        {BI_TEMPLATE_PARAM_BLOCK_ID, "TemplateParamBlock"},
        {BI_SPECIALIZATION_BLOCK_ID, "SpecializationBlock"},
        {BI_FRIEND_BLOCK_ID, "FriendBlock"},
        {BI_ENUMERATOR_BLOCK_ID, "EnumeratorBlock"},
        {BI_VARIABLE_BLOCK_ID, "VarBlock"},
        {BI_NAME_INFO_ID, "NameInfoBlock"},
        {BI_ALIAS_BLOCK_ID, "AliasBlock"},
        {BI_USING_BLOCK_ID, "UsingBlock"},
    };
    MRDOCS_ASSERT(Inits.size() == BlockIdCount);
    for (const auto& Init : Inits)
        BlockIdNameMap[Init.first] = Init.second;
    MRDOCS_ASSERT(BlockIdNameMap.size() == BlockIdCount);
    return BlockIdNameMap;
}();

static
llvm::IndexedMap<RecordIDDsc, RecordIDToIndexFunctor> const
RecordIDNameMap = []()
{
    llvm::IndexedMap<RecordIDDsc, RecordIDToIndexFunctor> RecordIDNameMap;
    RecordIDNameMap.resize(RecordIDCount);

    // There is no init-list constructor for the IndexedMap, so have to
    // improvise
    static const std::vector<std::pair<RecordID, RecordIDDsc>> Inits = {
        {VERSION, {"Version", &Integer32Abbrev}},
        {ALIAS_SYMBOL, {"AliasedSymbol", &SymbolIDAbbrev}},
        {BASE_ACCESS, {"BaseAccess", &Integer32Abbrev}},
        {BASE_IS_VIRTUAL, {"BaseIsVirtual", &BoolAbbrev}},
        {ENUM_SCOPED, {"Scoped", &BoolAbbrev}},
        {EXPR_WRITTEN, {"ExprWritten", &StringAbbrev}},
        {EXPR_VALUE, {"ExprValue", &Integer64Abbrev}},
        {FIELD_DEFAULT, {"DefaultValue", &StringAbbrev}},
        {FIELD_ATTRIBUTES, {"FieldAttributes", &Integer32ArrayAbbrev}},
        {FIELD_IS_MUTABLE, {"FieldIsMutable", &BoolAbbrev}},
        {FIELD_IS_BITFIELD, {"FieldIsBitfield", &BoolAbbrev}},
        {FRIEND_SYMBOL, {"FriendSymbol", &SymbolIDAbbrev}},
        {FUNCTION_BITS, {"Bits", &Integer32ArrayAbbrev}},
        {FUNCTION_CLASS, {"FunctionClass", &Integer32Abbrev}},
        {FUNCTION_NOEXCEPT, {"FunctionNoexcept", &NoexceptAbbrev}},
        {FUNCTION_PARAM_NAME, {"Name", &StringAbbrev}},
        {FUNCTION_PARAM_DEFAULT, {"Default", &StringAbbrev}},
        {GUIDE_EXPLICIT, {"Explicit", &Integer32Abbrev}},
        {INFO_PART_ACCESS, {"InfoAccess", &Integer32Abbrev}},
        {INFO_PART_ID, {"InfoID", &SymbolIDAbbrev}},
        {INFO_PART_IMPLICIT, {"InfoImplicit", &BoolAbbrev}},
        {INFO_PART_NAME, {"InfoName", &StringAbbrev}},
        {INFO_PART_PARENTS, {"InfoParents", &SymbolIDsAbbrev}},
        {JAVADOC_NODE_ADMONISH, {"JavadocNodeAdmonish", &Integer32Abbrev}},
        {JAVADOC_NODE_HREF, {"JavadocNodeHref", &StringAbbrev}},
        {JAVADOC_NODE_KIND, {"JavadocNodeKind", &Integer32Abbrev}},
        {JAVADOC_NODE_STRING, {"JavadocNodeString", &StringAbbrev}},
        {JAVADOC_NODE_STYLE, {"JavadocNodeStyle", &Integer32Abbrev}},
        {JAVADOC_NODE_PART, {"JavadocNodePart", &Integer32Abbrev}},
        {JAVADOC_NODE_SYMBOLREF, {"JavadocNodeSymbol", &SymbolIDAbbrev}},
        {JAVADOC_PARAM_DIRECTION, {"JavadocParamDirection", &Integer32Abbrev}},
        {NAMESPACE_BITS, {"NamespaceBits", &Integer32ArrayAbbrev}},
        {NAME_INFO_KIND, {"NameKind", &Integer32Abbrev}},
        {NAME_INFO_ID, {"NameID", &SymbolIDAbbrev}},
        {NAME_INFO_NAME, {"NameName", &StringAbbrev}},
        {RECORD_KEY_KIND, {"KeyKind", &Integer32Abbrev}},
        {RECORD_IS_TYPE_DEF, {"IsTypeDef", &BoolAbbrev}},
        {RECORD_BITS, {"Bits", &Integer32ArrayAbbrev}},
        {SPECIALIZATION_PRIMARY, {"SpecializationPrimary", &SymbolIDAbbrev}},
        {SCOPE_INFO_MEMBERS, {"ScopeMembers", &SymbolIDsAbbrev}},
        {LOOKUP_NAME, {"LookupName", &StringAbbrev}},
        {LOOKUP_MEMBERS, {"LookupMembers", &SymbolIDsAbbrev}},
        {SOURCE_INFO_DEFLOC, {"SourceDefLoc", &LocationAbbrev}},
        {SOURCE_INFO_LOC,    {"SourceLoc", &LocationAbbrev}},
        {TEMPLATE_PRIMARY_USR,   {"Primary", &SymbolIDAbbrev}},
        {TEMPLATE_ARG_KIND,      {"TArgKind", &Integer32Abbrev}},
        {TEMPLATE_ARG_IS_PACK,   {"IsPack", &BoolAbbrev}},
        {TEMPLATE_ARG_TEMPLATE,  {"TemplateID", &SymbolIDAbbrev}},
        {TEMPLATE_ARG_NAME,      {"TemplateName", &StringAbbrev}},
        {TEMPLATE_PARAM_KIND,    {"Kind", &Integer32Abbrev}},
        {TEMPLATE_PARAM_NAME,    {"Name", &StringAbbrev}},
        {TEMPLATE_PARAM_IS_PACK, {"IsPack", &BoolAbbrev}},
        {TEMPLATE_PARAM_KEY_KIND,{"TParamKeyKind", &Integer32Abbrev}},
        {TYPEINFO_KIND, {"TypeinfoKind", &Integer32Abbrev}},
        {TYPEINFO_IS_PACK, {"TypeinfoIsPack", &BoolAbbrev}},
        {TYPEINFO_CVQUAL, {"TypeinfoCV", &Integer32Abbrev}},
        {TYPEINFO_NOEXCEPT, {"TypeinfoNoexcept", &NoexceptAbbrev}},
        {TYPEINFO_REFQUAL, {"TypeinfoRefqual", &Integer32Abbrev}},
        {TYPEDEF_IS_USING, {"IsUsing", &BoolAbbrev}},
        {VARIABLE_BITS, {"Bits", &Integer32ArrayAbbrev}},
        {USING_SYMBOLS, {"UsingSymbols", &SymbolIDsAbbrev}},
        {USING_CLASS, {"UsingClass", &Integer32Abbrev}},
    };
    // MRDOCS_ASSERT(Inits.size() == RecordIDCount);
    for (const auto& Init : Inits)
    {
        RecordIDNameMap[Init.first] = Init.second;
        MRDOCS_ASSERT((Init.second.Name.size() + 1) <= BitCodeConstants::RecordSize);
    }
    // MRDOCS_ASSERT(RecordIDNameMap.size() == RecordIDCount);
    return RecordIDNameMap;
}();

//------------------------------------------------

static
std::vector<std::pair<BlockID, std::vector<RecordID>>> const
RecordsByBlock{
    // Version Block
    {BI_VERSION_BLOCK_ID, {VERSION}},
    // Info part
    {BI_INFO_PART_ID,
        {INFO_PART_ID, INFO_PART_ACCESS, INFO_PART_IMPLICIT,
         INFO_PART_NAME, INFO_PART_PARENTS}},
    // SourceInfo
    {BI_SOURCE_INFO_ID,
        {SOURCE_INFO_DEFLOC, SOURCE_INFO_LOC}},
    // ScopeInfo
    {BI_SCOPE_INFO_ID,
        {SCOPE_INFO_MEMBERS}},
    // Lookup entry
    {BI_LOOKUP_INFO_ID,
        {LOOKUP_NAME, LOOKUP_MEMBERS}},
    // BaseInfo
    {BI_BASE_BLOCK_ID,
        {BASE_ACCESS, BASE_IS_VIRTUAL}},
    // EnumInfo
    {BI_ENUM_BLOCK_ID,
        {ENUM_SCOPED}},
    // ExprInfo and ConstantExprInfo
    {BI_EXPR_BLOCK_ID,
        {EXPR_WRITTEN, EXPR_VALUE}},
    {BI_BITFIELD_WIDTH_BLOCK_ID,
        {}},
    // FieldInfo
    {BI_FIELD_BLOCK_ID,
        {FIELD_DEFAULT, FIELD_ATTRIBUTES,
        FIELD_IS_MUTABLE, FIELD_IS_BITFIELD}},
    // FunctionInfo
    {BI_FUNCTION_BLOCK_ID,
        {FUNCTION_BITS, FUNCTION_CLASS, FUNCTION_NOEXCEPT}},
    // Param
    {BI_FUNCTION_PARAM_BLOCK_ID,
        {FUNCTION_PARAM_NAME, FUNCTION_PARAM_DEFAULT}},
    // Javadoc
    {BI_JAVADOC_BLOCK_ID,
        {}},
    // doc::List<doc::Node>
    {BI_JAVADOC_LIST_BLOCK_ID,
        {}},
    // doc::Node
    {BI_JAVADOC_NODE_BLOCK_ID,
        {JAVADOC_NODE_KIND, JAVADOC_NODE_HREF, JAVADOC_NODE_STRING,
        JAVADOC_NODE_STYLE, JAVADOC_NODE_ADMONISH, JAVADOC_PARAM_DIRECTION,
        JAVADOC_NODE_PART, JAVADOC_NODE_SYMBOLREF}},
    // NamespaceInfo
    {BI_NAMESPACE_BLOCK_ID,
        {NAMESPACE_BITS}},
    // RecordInfo
    {BI_RECORD_BLOCK_ID,
        {RECORD_KEY_KIND, RECORD_IS_TYPE_DEF, RECORD_BITS}},
    // TArg
    {BI_TEMPLATE_ARG_BLOCK_ID,
        {TEMPLATE_ARG_KIND, TEMPLATE_ARG_IS_PACK,
        TEMPLATE_ARG_TEMPLATE, TEMPLATE_ARG_NAME}},
    // TemplateInfo
    {BI_TEMPLATE_BLOCK_ID,
        {TEMPLATE_PRIMARY_USR}},
    // TParam
    {BI_TEMPLATE_PARAM_BLOCK_ID,
        {TEMPLATE_PARAM_KIND, TEMPLATE_PARAM_NAME,
        TEMPLATE_PARAM_IS_PACK, TEMPLATE_PARAM_KEY_KIND}},
    // SpecializationInfo
    {BI_SPECIALIZATION_BLOCK_ID,
        {SPECIALIZATION_PRIMARY}},
    // FriendInfo
    {BI_FRIEND_BLOCK_ID,
        {FRIEND_SYMBOL}},
    // FriendInfo


    // AliasInfo
    {BI_ALIAS_BLOCK_ID,
        {ALIAS_SYMBOL}},


    // UsingInfo
    {BI_USING_BLOCK_ID,
        {USING_SYMBOLS, USING_CLASS}},

    // EnumeratorInfo
    {BI_ENUMERATOR_BLOCK_ID,
        {}},
    // TypeInfo
    {BI_TYPEINFO_BLOCK_ID,
        {TYPEINFO_KIND, TYPEINFO_IS_PACK, TYPEINFO_CVQUAL,
        TYPEINFO_NOEXCEPT, TYPEINFO_REFQUAL}},
    {BI_TYPEINFO_PARENT_BLOCK_ID,
        {}},
    {BI_TYPEINFO_CHILD_BLOCK_ID,
        {}},
    {BI_TYPEINFO_PARAM_BLOCK_ID,
        {}},
    // TypedefInfo
    {BI_TYPEDEF_BLOCK_ID,
        {TYPEDEF_IS_USING}},
    // VariableInfo
    {BI_VARIABLE_BLOCK_ID, {VARIABLE_BITS}},
    // GuideInfo
    {BI_GUIDE_BLOCK_ID, {GUIDE_EXPLICIT}},
    {BI_NAME_INFO_ID,
        {NAME_INFO_KIND, NAME_INFO_ID, NAME_INFO_NAME}},
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
dispatchInfoForWrite(const Info& I)
{
    visit(I, [this](const auto& info)
        {
            emitBlock(info);
        });
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
        MRDOCS_ASSERT(Block.second.size() < (1U << BitCodeConstants::SubblockIDSize));
        emitBlockInfo(Block.first, Block.second);
    }
    Stream.ExitBlock();
}

void
BitcodeWriter::
emitVersionBlock()
{
    StreamSubBlockGuard Block(Stream, BI_VERSION_BLOCK_ID);
    emitRecord(BitcodeVersion, VERSION);
}

// AbbreviationMap

constexpr unsigned char BitCodeConstants::Signature[];

void
BitcodeWriter::
AbbreviationMap::
add(RecordID RID,
    unsigned AbbrevID)
{
    MRDOCS_ASSERT(RecordIDNameMap[RID] && "Unknown RecordID.");
    //MRDOCS_ASSERT((Abbrevs.find(RID) == Abbrevs.end()) && "Abbreviation already added.");
    Abbrevs[RID] = AbbrevID;
}

unsigned
BitcodeWriter::
AbbreviationMap::
get(RecordID RID) const
{
    MRDOCS_ASSERT(RecordIDNameMap[RID] && "Unknown RecordID.");
    MRDOCS_ASSERT((Abbrevs.find(RID) != Abbrevs.end()) && "Unknown abbreviation.");
    return Abbrevs.lookup(RID);
}

/// Emits a block ID and the block name to the BLOCKINFO block.
void
BitcodeWriter::
emitBlockID(BlockID BID)
{
    const auto& BlockIdName = BlockIdNameMap[BID];
    MRDOCS_ASSERT(BlockIdName.data() && BlockIdName.size() && "Unknown BlockID.");

    Record.clear();
    Record.push_back(BID);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETBID, Record);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_BLOCKNAME,
        llvm::ArrayRef<unsigned char>(BlockIdName.bytes_begin(),
            BlockIdName.bytes_end()));
}

/// Emits a record name to the BLOCKINFO block.
void
BitcodeWriter::
emitRecordID(RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    prepRecordData(ID);
    Record.append(RecordIDNameMap[ID].Name.begin(),
        RecordIDNameMap[ID].Name.end());
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETRECORDNAME, Record);
}

// Abbreviations

void
BitcodeWriter::
emitAbbrev(
    RecordID ID, BlockID Block)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown abbreviation.");
    auto Abbrev = std::make_shared<llvm::BitCodeAbbrev>();
    Abbrev->Add(llvm::BitCodeAbbrevOp(ID));
    RecordIDNameMap[ID].Abbrev(Abbrev);
    Abbrevs.add(ID, Stream.EmitBlockInfoAbbrev(Block, std::move(Abbrev)));
}

//------------------------------------------------
//
// Records
//
//------------------------------------------------

// Integer
template<class Integer>
requires std::integral<Integer>
void
BitcodeWriter::
emitRecord(
    Integer Value, RecordID ID)
{
    constexpr std::size_t width = sizeof(Integer);
    static_assert(width == 4 || width == 8,
        "unsupported integer width");

    MRDOCS_ASSERT(RecordIDNameMap[ID]);
    if (!prepRecordData(ID, Value))
        return;
    if constexpr(width == 8)
    {
        MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer64Abbrev);
        Record.push_back(static_cast<RecordValue>(Value));
        Record.push_back(static_cast<RecordValue>(
            static_cast<std::uint64_t>(Value) >> 32));
    }
    else if constexpr(width == 4)
    {
        MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer32Abbrev);
        Record.push_back(static_cast<RecordValue>(Value));
    }
#if 0
    else if constexpr(width == 2)
    {
        MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer16Abbrev);
    }
#endif


    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

// enum
template<class Enum>
requires std::is_enum_v<Enum>
void
BitcodeWriter::
emitRecord(
    Enum Value, RecordID ID)
{
    emitRecord(static_cast<std::underlying_type_t<Enum>>(Value), ID);
}

// Bits
void
BitcodeWriter::
emitRecord(
    std::initializer_list<BitFieldFullValue> values,
    RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID]);
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer32ArrayAbbrev);
    if (!prepRecordData(ID, true))
        return;
    Record.push_back(values.size());
    for(std::uint32_t value : values)
        Record.push_back(value);
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

// SymbolIDs
void
BitcodeWriter::
emitRecord(
    std::vector<SymbolID> const& Values, RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID]);
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &SymbolIDsAbbrev);
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
    RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &SymbolIDAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, !! Sym))
        return;
    MRDOCS_ASSERT(Sym.size() == 20);
    Record.push_back(Sym.size());
    Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    llvm::StringRef Str, RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &StringAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, !Str.empty()))
        return;
    MRDOCS_ASSERT(Str.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Str.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Str);
}

void
BitcodeWriter::
emitRecord(
    Location const& Loc, RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &LocationAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, true))
        return;
    // FIXME: Assert that the line number
    // is of the appropriate size.
    Record.push_back(Loc.LineNumber);
    Record.push_back(to_underlying(Loc.Kind));
    Record.push_back(Loc.Documented);
    MRDOCS_ASSERT(Loc.Path.size() + Loc.Filename.size() <
        (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Loc.Path.size());
    Record.push_back(Loc.Path.size() + Loc.Filename.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record,
        Loc.Path + Loc.Filename);
}

void
BitcodeWriter::
emitRecord(
    NoexceptInfo const& I, RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &NoexceptAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, true))
        return;

    Record.push_back(I.Implicit);
    Record.push_back(to_underlying(I.Kind));
    Record.push_back(I.Operand.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, I.Operand);
}

void
BitcodeWriter::
emitRecord(
    bool Val, RecordID ID)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOCS_ASSERT(RecordIDNameMap[ID].Abbrev == &BoolAbbrev && "Abbrev type mismatch.");
    if (!prepRecordData(ID, Val))
        return;
    Record.push_back(Val);
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

bool
BitcodeWriter::
prepRecordData(
    RecordID ID, bool ShouldEmit)
{
    MRDOCS_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
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
    BlockID BID,
    std::vector<RecordID> const& RIDs)
{
    MRDOCS_ASSERT(RIDs.size() < (1U << BitCodeConstants::SubblockIDSize));
    emitBlockID(BID);
    for (RecordID RID : RIDs)
    {
        emitRecordID(RID);
        emitAbbrev(RID, BID);
    }
}

//------------------------------------------------
//
// emitBlock
//
//------------------------------------------------

template<class T>
void
BitcodeWriter::
emitBlock(
    const doc::List<T>& list)
{
    StreamSubBlockGuard Block(
        Stream, BI_JAVADOC_LIST_BLOCK_ID);
    for(const auto& node : list)
        emitBlock(*node);
}

void
BitcodeWriter::
emitInfoPart(
    Info const& I)
{
    StreamSubBlockGuard Block(Stream, BI_INFO_PART_ID);
    emitRecord(I.id, INFO_PART_ID);
    emitRecord(I.Access, INFO_PART_ACCESS);
    emitRecord(I.Implicit, INFO_PART_IMPLICIT);
    emitRecord(I.Name, INFO_PART_NAME);
    emitRecord(I.Namespace, INFO_PART_PARENTS);
    emitBlock(I.javadoc);
}

void
BitcodeWriter::
emitSourceInfo(
    const SourceInfo& S)
{
    StreamSubBlockGuard Block(Stream, BI_SOURCE_INFO_ID);
    if(S.DefLoc)
        emitRecord(*S.DefLoc, SOURCE_INFO_DEFLOC);
    for(const auto& L : S.Loc)
        emitRecord(L, SOURCE_INFO_LOC);
}

void
BitcodeWriter::
emitScopeInfo(
    const ScopeInfo& S)
{
    StreamSubBlockGuard Block(Stream, BI_SCOPE_INFO_ID);
    emitRecord(S.Members, SCOPE_INFO_MEMBERS);
    for(const auto& [name, symbols] : S.Lookups)
        emitLookup(name, symbols);
}

void
BitcodeWriter::
emitLookup(llvm::StringRef Name, std::vector<SymbolID> const& Members)
{
    StreamSubBlockGuard Block(Stream, BI_LOOKUP_INFO_ID);
    emitRecord(Name, LOOKUP_NAME);
    emitRecord(Members, LOOKUP_MEMBERS);
}

void
BitcodeWriter::
emitBlock(
    BaseInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_BASE_BLOCK_ID);
    emitRecord(I.Access, BASE_ACCESS);
    emitRecord(I.IsVirtual, BASE_IS_VIRTUAL);
    emitBlock(I.Type);
}

void
BitcodeWriter::
emitBlock(
    FieldInfo const& F)
{
    StreamSubBlockGuard Block(Stream, BI_FIELD_BLOCK_ID);
    emitInfoPart(F);
    emitSourceInfo(F);
    emitBlock(F.Type);
    emitBlock(F.Default);
    emitRecord({F.specs.raw}, FIELD_ATTRIBUTES);
    emitRecord(F.IsMutable, FIELD_IS_MUTABLE);
    emitRecord(F.IsBitfield, FIELD_IS_BITFIELD);
    emitBlock(F.BitfieldWidth, BI_BITFIELD_WIDTH_BLOCK_ID);
}

void
BitcodeWriter::
emitBlock(
    const Param& P)
{
    StreamSubBlockGuard Block(Stream, BI_FUNCTION_PARAM_BLOCK_ID);
    emitRecord(P.Name, FUNCTION_PARAM_NAME);
    emitRecord(P.Default, FUNCTION_PARAM_DEFAULT);
    emitBlock(P.Type);
}

void
BitcodeWriter::
emitBlock(
    FunctionInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_FUNCTION_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord({I.specs0.raw, I.specs1.raw}, FUNCTION_BITS);
    emitRecord(I.Class, FUNCTION_CLASS);
    emitBlock(I.ReturnType);
    for (const auto& N : I.Params)
        emitBlock(N);
    emitRecord(I.Noexcept, FUNCTION_NOEXCEPT);
}

void
BitcodeWriter::
emitBlock(
    GuideInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_GUIDE_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord(I.Explicit, GUIDE_EXPLICIT);
    emitBlock(I.Deduced);
    for (const auto& N : I.Params)
        emitBlock(N);
}

void
BitcodeWriter::
emitBlock(
    std::unique_ptr<Javadoc> const& jd)
{
    if(! jd)
        return;
    // If the unique_ptr<Javadoc> has a value then we
    // always want to emit it, even if it is empty.
    StreamSubBlockGuard Block(Stream, BI_JAVADOC_BLOCK_ID);
    emitBlock(jd->getBlocks());
}

void
BitcodeWriter::
emitBlock(
    doc::Node const& I)
{
    StreamSubBlockGuard Block(
        Stream, BI_JAVADOC_NODE_BLOCK_ID);
    emitRecord(I.kind, JAVADOC_NODE_KIND);
    visit(I, [&]<typename NodeTy>(const NodeTy& J)
    {
        if constexpr(requires { J.href; })
            emitRecord(J.href, JAVADOC_NODE_HREF);
        if constexpr(requires { J.string; })
            emitRecord(J.string, JAVADOC_NODE_STRING);
        if constexpr(requires { J.style; })
            emitRecord(J.style, JAVADOC_NODE_STYLE);
        if constexpr(requires { J.admonish; })
            emitRecord(J.admonish, JAVADOC_NODE_ADMONISH);
        if constexpr(requires { J.direction; })
            emitRecord(J.direction, JAVADOC_PARAM_DIRECTION);
        if constexpr(requires { J.parts; })
            emitRecord(J.parts, JAVADOC_NODE_PART);
        if constexpr(requires { J.id; })
            emitRecord(J.id, JAVADOC_NODE_SYMBOLREF);
        if constexpr(requires { J.name; })
            emitRecord(J.name, JAVADOC_NODE_STRING);
        if constexpr(requires { J.exception; })
            emitRecord(J.exception, JAVADOC_NODE_STRING);
        if constexpr(requires { J.children; })
            emitBlock(J.children);
    });
}

template<typename ExprInfoTy>
    requires std::derived_from<ExprInfoTy, ExprInfo>
void
BitcodeWriter::
emitBlock(
    ExprInfoTy const& E)
{
    StreamSubBlockGuard Block(Stream, BI_EXPR_BLOCK_ID);
    emitRecord(E.Written, EXPR_WRITTEN);
    if constexpr(requires { E.Value; })
    {
        if(E.Value)
            emitRecord(*E.Value, EXPR_VALUE);
    }
}

template<typename ExprInfoTy>
    requires std::derived_from<ExprInfoTy, ExprInfo>
void
BitcodeWriter::
emitBlock(
    ExprInfoTy const& E,
    BlockID ID)
{
    StreamSubBlockGuard Block(Stream, ID);
    emitBlock(E);
}

void
BitcodeWriter::
emitBlock(
    std::unique_ptr<TypeInfo> const& TI,
    BlockID ID)
{
    if(! TI)
        return;
    StreamSubBlockGuard Block(Stream, ID);
    emitBlock(TI);
}

void
BitcodeWriter::
emitBlock(
    std::unique_ptr<TypeInfo> const& TI)
{
    if(! TI)
        return;
    // If the unique_ptr<Javadoc> has a value then we
    // always want to emit it, even if it is empty.
    StreamSubBlockGuard Block(Stream, BI_TYPEINFO_BLOCK_ID);

    emitRecord(TI->Kind, TYPEINFO_KIND);
    emitRecord(TI->IsPackExpansion, TYPEINFO_IS_PACK);
    visit(*TI, [&]<typename T>(const T& t)
    {
        if constexpr(requires { t.CVQualifiers; })
            emitRecord(t.CVQualifiers, TYPEINFO_CVQUAL);

        if constexpr(requires { t.ParentType; })
            emitBlock(t.ParentType, BI_TYPEINFO_PARENT_BLOCK_ID);

        if constexpr(requires { t.PointeeType; })
            emitBlock(t.PointeeType, BI_TYPEINFO_CHILD_BLOCK_ID);

        if constexpr(T::isArray())
        {
            emitBlock(t.ElementType, BI_TYPEINFO_CHILD_BLOCK_ID);
            emitBlock(t.Bounds);
        }

        if constexpr(T::isDecltype())
        {
            emitBlock(t.Operand);
        }

        if constexpr(T::isFunction())
        {
            emitBlock(t.ReturnType, BI_TYPEINFO_CHILD_BLOCK_ID);
            for(const auto& P : t.ParamTypes)
                emitBlock(P, BI_TYPEINFO_PARAM_BLOCK_ID);

            emitRecord(t.RefQualifier, TYPEINFO_REFQUAL);
            emitRecord(t.ExceptionSpec, TYPEINFO_NOEXCEPT);
        }

        if constexpr(T::isNamed())
        {
            if(t.Name)
                emitBlock(*t.Name);
        }
    });
}

void
BitcodeWriter::
emitBlock(
    NamespaceInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_NAMESPACE_BLOCK_ID);
    emitInfoPart(I);
    emitScopeInfo(I);
    emitRecord({I.specs.raw}, NAMESPACE_BITS);
}

void
BitcodeWriter::
emitBlock(
    RecordInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_RECORD_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitScopeInfo(I);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord(I.KeyKind, RECORD_KEY_KIND);
    emitRecord(I.IsTypeDef, RECORD_IS_TYPE_DEF);
    emitRecord({I.specs.raw}, RECORD_BITS);
    for (const auto& B : I.Bases)
        emitBlock(B);
}

void
BitcodeWriter::
emitBlock(
    EnumInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_ENUM_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitScopeInfo(I);
    emitRecord(I.Scoped, ENUM_SCOPED);
    emitBlock(I.UnderlyingType);
}

void
BitcodeWriter::
emitBlock(
    const SpecializationInfo& I)
{
    StreamSubBlockGuard Block(Stream, BI_SPECIALIZATION_BLOCK_ID);
    emitInfoPart(I);
    emitScopeInfo(I);
    emitRecord(I.Primary, SPECIALIZATION_PRIMARY);
    for(const auto& targ : I.Args)
        emitBlock(targ);
}


void
BitcodeWriter::
emitBlock(
    const FriendInfo& I)
{
    StreamSubBlockGuard Block(Stream, BI_FRIEND_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitRecord(I.FriendSymbol, FRIEND_SYMBOL);
    emitBlock(I.FriendType);
}

void
BitcodeWriter::
emitBlock(
    AliasInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_ALIAS_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitRecord(I.AliasedSymbol, ALIAS_SYMBOL);
    if (I.FullyQualifiedName)
        emitBlock(*I.FullyQualifiedName);
}

void
BitcodeWriter::
emitBlock(
    UsingInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_USING_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitRecord(I.UsingSymbols, USING_SYMBOLS);
    if (I.Qualifier)
        emitBlock(*I.Qualifier);
    emitRecord(I.Class, USING_CLASS);
}

void
BitcodeWriter::
emitBlock(
    const EnumeratorInfo& I)
{
    StreamSubBlockGuard Block(Stream, BI_ENUMERATOR_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitBlock(I.Initializer);
}

void
BitcodeWriter::
emitBlock(
    const TemplateInfo& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_BLOCK_ID);
    emitRecord(T.Primary, TEMPLATE_PRIMARY_USR);
    for(const auto& targ : T.Args)
        emitBlock(targ);
    for(const auto& tparam : T.Params)
        emitBlock(tparam);
}

void
BitcodeWriter::
emitBlock(NameInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_NAME_INFO_ID);
    emitRecord(I.Kind, NAME_INFO_KIND);
    emitRecord(I.id, NAME_INFO_ID);
    emitRecord(I.Name, NAME_INFO_NAME);
    if(I.Prefix)
        emitBlock(*I.Prefix);
    if(I.Kind == NameKind::Specialization)
    {
        for(const auto& targ : static_cast<
            const SpecializationNameInfo&>(I).TemplateArgs)
            emitBlock(targ);
    }
}

void
BitcodeWriter::
emitBlock(
    const std::unique_ptr<TParam>& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_PARAM_BLOCK_ID);
    visit(*T, [&]<typename T>(const T& P)
    {
        emitRecord(P.Kind, TEMPLATE_PARAM_KIND);
        emitRecord(P.Name, TEMPLATE_PARAM_NAME);
        emitRecord(P.IsParameterPack, TEMPLATE_PARAM_IS_PACK);

        if(P.Default)
            emitBlock(P.Default);

        if constexpr(T::isType())
        {
            emitRecord(P.KeyKind, TEMPLATE_PARAM_KEY_KIND);
        }
        if constexpr(T::isNonType())
        {
            emitBlock(P.Type);
        }
        if constexpr(T::isTemplate())
        {
            for(const auto& P : P.Params)
                emitBlock(P);
        }
    });
}

void
BitcodeWriter::
emitBlock(
    const std::unique_ptr<TArg>& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_ARG_BLOCK_ID);
    visit(*T, [&]<typename T>(const T& A)
    {
        emitRecord(A.Kind, TEMPLATE_ARG_KIND);
        emitRecord(A.IsPackExpansion, TEMPLATE_ARG_IS_PACK);

        if constexpr(T::isType())
        {
            emitBlock(A.Type);
        }
        else if constexpr(T::isNonType())
        {
            emitBlock(A.Value);
        }
        else if constexpr(T::isTemplate())
        {
            emitRecord(A.Template, TEMPLATE_ARG_TEMPLATE);
            emitRecord(A.Name, TEMPLATE_ARG_NAME);
        }
    });
}

void
BitcodeWriter::
emitBlock(
    TypedefInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_TYPEDEF_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    emitRecord(I.IsUsing, TYPEDEF_IS_USING);
    emitBlock(I.Type);
    if(I.Template)
        emitBlock(*I.Template);
}

void
BitcodeWriter::
emitBlock(
    VariableInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_VARIABLE_BLOCK_ID);
    emitInfoPart(I);
    emitSourceInfo(I);
    if(I.Template)
        emitBlock(*I.Template);
    emitBlock(I.Type);
    emitBlock(I.Initializer);
    emitRecord({I.specs.raw}, VARIABLE_BITS);
}

//------------------------------------------------

llvm::SmallString<0>
writeBitcode(Info const& info)
{
    llvm::SmallString<0> buffer;
    llvm::BitstreamWriter stream(buffer);
    BitcodeWriter writer(stream);
    writer.dispatchInfoForWrite(info);
    return buffer;
}

} // mrdocs
} // clang
