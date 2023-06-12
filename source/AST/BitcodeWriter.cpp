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

#include "BitcodeWriter.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include "Support/Debug.hpp"
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
        {BI_SYMBOL_PART_ID, "SymbolPart"},
        {BI_NAMESPACE_BLOCK_ID, "NamespaceBlock"},
        {BI_ENUM_BLOCK_ID, "EnumBlock"},
        {BI_ENUM_VALUE_BLOCK_ID, "EnumValueBlock"},
        {BI_TYPEDEF_BLOCK_ID, "TypedefBlock"},
        {BI_TYPE_BLOCK_ID, "TypeBlock"},
        {BI_FIELD_BLOCK_ID, "FieldBlock"},
        {BI_RECORD_BLOCK_ID, "RecordBlock"},
        {BI_FUNCTION_BLOCK_ID, "FunctionBlock"},
        {BI_FUNCTION_PARAM_BLOCK_ID, "FunctionParamBlock"},
        {BI_JAVADOC_BLOCK_ID, "JavadocBlock"},
        {BI_JAVADOC_LIST_BLOCK_ID, "JavadocListBlock"},
        {BI_JAVADOC_NODE_BLOCK_ID, "JavadocNodeBlock"},
        {BI_TEMPLATE_ARG_BLOCK_ID, "TemplateArgBlock"},
        {BI_TEMPLATE_BLOCK_ID, "TemplateBlock"},
        {BI_TEMPLATE_PARAM_BLOCK_ID, "TemplateParamBlock"},
        {BI_SPECIALIZATION_BLOCK_ID, "SpecializationBlock"},
        {BI_VARIABLE_BLOCK_ID, "VarBlock"}
    };
    MRDOX_ASSERT(Inits.size() == BlockIdCount);
    for (const auto& Init : Inits)
        BlockIdNameMap[Init.first] = Init.second;
    MRDOX_ASSERT(BlockIdNameMap.size() == BlockIdCount);
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
        {BASE_ID, {"BaseID", &SymbolIDAbbrev}},
        {BASE_NAME, {"BaseName", &StringAbbrev}},
        {BASE_ACCESS, {"BaseAccess", &Integer32Abbrev}},
        {BASE_IS_VIRTUAL, {"BaseIsVirtual", &BoolAbbrev}},
        {ENUM_SCOPED, {"Scoped", &BoolAbbrev}},
        {ENUM_VALUE_NAME, {"Name", &StringAbbrev}},
        {ENUM_VALUE_VALUE, {"Value", &StringAbbrev}},
        {ENUM_VALUE_EXPR, {"Expr", &StringAbbrev}},
        {FIELD_NAME, {"Name", &StringAbbrev}},
        {FIELD_DEFAULT, {"DefaultValue", &StringAbbrev}},
        {FIELD_ATTRIBUTES, {"FieldAttributes", &Integer32ArrayAbbrev}},
        {FUNCTION_BITS, {"Bits", &Integer32ArrayAbbrev}},
        {FUNCTION_PARAM_NAME, {"Name", &StringAbbrev}},
        {FUNCTION_PARAM_DEFAULT, {"Default", &StringAbbrev}},
        {INFO_PART_ACCESS, {"InfoAccess", &Integer32Abbrev}},
        {INFO_PART_ID, {"InfoID", &SymbolIDAbbrev}},
        {INFO_PART_NAME, {"InfoName", &StringAbbrev}},
        {INFO_PART_PARENTS, {"InfoParents", &SymbolIDsAbbrev}},
        {JAVADOC_LIST_KIND, {"JavadocListKind", &Integer32Abbrev}},
        {JAVADOC_NODE_KIND, {"JavadocNodeKind", &Integer32Abbrev}},
        {JAVADOC_NODE_STRING, {"JavadocNodeString", &StringAbbrev}},
        {JAVADOC_NODE_STYLE, {"JavadocNodeStyle", &Integer32Abbrev}},
        {JAVADOC_NODE_ADMONISH, {"JavadocNodeAdmonish", &Integer32Abbrev}},
        {JAVADOC_PARAM_DIRECTION, {"JavadocParamDirection", &Integer32Abbrev}},
        {NAMESPACE_MEMBERS, {"NamespaceMembers", &SymbolIDsAbbrev}},
        {NAMESPACE_SPECIALIZATIONS, {"NamespaceSpecializations", &SymbolIDsAbbrev}},
        {RECORD_KEY_KIND, {"KeyKind", &Integer32Abbrev}},
        {RECORD_IS_TYPE_DEF, {"IsTypeDef", &BoolAbbrev}},
        {RECORD_BITS, {"Bits", &Integer32ArrayAbbrev}},
        {RECORD_FRIENDS, {"Friends", &SymbolIDsAbbrev}},
        {RECORD_MEMBERS, {"RecordMembers", &SymbolIDsAbbrev}},
        {RECORD_SPECIALIZATIONS, {"RecordSpecializations", &SymbolIDsAbbrev}},
        {SPECIALIZATION_PRIMARY, {"SpecializationPrimary", &SymbolIDAbbrev}},
        {SPECIALIZATION_MEMBERS, {"SpecializationMembers", &SymbolIDsAbbrev}},
        {SYMBOL_PART_DEFLOC, {"SymbolDefLoc", &LocationAbbrev}},
        {SYMBOL_PART_LOC,    {"SymbolLoc", &LocationAbbrev}},
        {TEMPLATE_PRIMARY_USR,   {"Primary", &SymbolIDAbbrev}},
        {TEMPLATE_ARG_VALUE,     {"Value", &StringAbbrev}},
        {TEMPLATE_PARAM_KIND,    {"Kind", &Integer32Abbrev}},
        {TEMPLATE_PARAM_NAME,    {"Name", &StringAbbrev}},
        {TEMPLATE_PARAM_IS_PACK, {"IsPack", &BoolAbbrev}},
        {TEMPLATE_PARAM_DEFAULT, {"Default", &StringAbbrev}},
        {TYPE_ID, {"TypeID", &SymbolIDAbbrev}},
        {TYPE_NAME, {"TypeName", &StringAbbrev}},
        {TYPEDEF_IS_USING, {"IsUsing", &BoolAbbrev}},
        {VARIABLE_BITS, {"Bits", &Integer32ArrayAbbrev}}
    };
    MRDOX_ASSERT(Inits.size() == RecordIDCount);
    for (const auto& Init : Inits)
    {
        RecordIDNameMap[Init.first] = Init.second;
        MRDOX_ASSERT((Init.second.Name.size() + 1) <= BitCodeConstants::RecordSize);
    }
    MRDOX_ASSERT(RecordIDNameMap.size() == RecordIDCount);
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
        {INFO_PART_ID, INFO_PART_ACCESS,
         INFO_PART_NAME, INFO_PART_PARENTS}},
    // SymbolInfo
    {BI_SYMBOL_PART_ID,
        {SYMBOL_PART_DEFLOC, SYMBOL_PART_LOC}},
    // BaseInfo
    {BI_BASE_BLOCK_ID,
        {BASE_ID, BASE_NAME, BASE_ACCESS, BASE_IS_VIRTUAL}},
    // EnumInfo
    {BI_ENUM_BLOCK_ID,
        {ENUM_SCOPED}},
    // EnumValue
    {BI_ENUM_VALUE_BLOCK_ID,
        {ENUM_VALUE_NAME, ENUM_VALUE_VALUE, ENUM_VALUE_EXPR}},
    // FieldInfo
    {BI_FIELD_BLOCK_ID,
        {FIELD_NAME, FIELD_DEFAULT, FIELD_ATTRIBUTES}},
    // FunctionInfo
    {BI_FUNCTION_BLOCK_ID,
        {FUNCTION_BITS}},
    // Param
    {BI_FUNCTION_PARAM_BLOCK_ID,
        {FUNCTION_PARAM_NAME, FUNCTION_PARAM_DEFAULT}},
    // Javadoc
    {BI_JAVADOC_BLOCK_ID,
        {}},
    // AnyList<Javadoc::Node>
    {BI_JAVADOC_LIST_BLOCK_ID,
        {JAVADOC_LIST_KIND}},
    // Javadoc::Node
    {BI_JAVADOC_NODE_BLOCK_ID,
        {JAVADOC_NODE_KIND, JAVADOC_NODE_STRING, JAVADOC_NODE_STYLE,
         JAVADOC_NODE_ADMONISH, JAVADOC_PARAM_DIRECTION}},
    // NamespaceInfo
    {BI_NAMESPACE_BLOCK_ID,
        {NAMESPACE_MEMBERS, NAMESPACE_SPECIALIZATIONS}},
    // RecordInfo
    {BI_RECORD_BLOCK_ID,
        {RECORD_KEY_KIND, RECORD_IS_TYPE_DEF, RECORD_BITS,
        RECORD_FRIENDS, RECORD_MEMBERS, RECORD_SPECIALIZATIONS}},
    // TArg
    {BI_TEMPLATE_ARG_BLOCK_ID,
        {TEMPLATE_ARG_VALUE}},
    // TemplateInfo
    {BI_TEMPLATE_BLOCK_ID,
        {TEMPLATE_PRIMARY_USR}},
    // TParam
    {BI_TEMPLATE_PARAM_BLOCK_ID,
        {TEMPLATE_PARAM_KIND, TEMPLATE_PARAM_NAME,
        TEMPLATE_PARAM_IS_PACK, TEMPLATE_PARAM_DEFAULT}},
    // SpecializationInfo
    {BI_SPECIALIZATION_BLOCK_ID,
        {SPECIALIZATION_PRIMARY, SPECIALIZATION_MEMBERS}},
    // TypeInfo
    {BI_TYPE_BLOCK_ID,
        {TYPE_ID, TYPE_NAME}},
    // TypedefInfo
    {BI_TYPEDEF_BLOCK_ID,
        {TYPEDEF_IS_USING}},
    // VarInfo
    {BI_VARIABLE_BLOCK_ID, {VARIABLE_BITS}}
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
    switch (I->Kind)
    {
    case InfoKind::Namespace:
        emitBlock(*static_cast<NamespaceInfo const*>(I));
        break;
    case InfoKind::Record:
        emitBlock(*static_cast<RecordInfo const*>(I));
        break;
    case InfoKind::Function:
        emitBlock(*static_cast<FunctionInfo const*>(I));
        break;
    case InfoKind::Enum:
        emitBlock(*static_cast<EnumInfo const*>(I));
        break;
    case InfoKind::Typedef:
        emitBlock(*static_cast<TypedefInfo const*>(I));
        break;
    case InfoKind::Variable:
        emitBlock(*static_cast<VarInfo const*>(I));
        break;
    case InfoKind::Field:
        emitBlock(*static_cast<FieldInfo const*>(I));
        break;
    case InfoKind::Specialization:
        emitBlock(*static_cast<SpecializationInfo const*>(I));
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
        MRDOX_ASSERT(Block.second.size() < (1U << BitCodeConstants::SubblockIDSize));
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
    MRDOX_ASSERT(RecordIDNameMap[RID] && "Unknown RecordID.");
    //MRDOX_ASSERT((Abbrevs.find(RID) == Abbrevs.end()) && "Abbreviation already added.");
    Abbrevs[RID] = AbbrevID;
}

unsigned
BitcodeWriter::
AbbreviationMap::
get(RecordID RID) const
{
    MRDOX_ASSERT(RecordIDNameMap[RID] && "Unknown RecordID.");
    MRDOX_ASSERT((Abbrevs.find(RID) != Abbrevs.end()) && "Unknown abbreviation.");
    return Abbrevs.lookup(RID);
}

/// Emits a block ID and the block name to the BLOCKINFO block.
void
BitcodeWriter::
emitBlockID(BlockID BID)
{
    const auto& BlockIdName = BlockIdNameMap[BID];
    MRDOX_ASSERT(BlockIdName.data() && BlockIdName.size() && "Unknown BlockID.");

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
emitRecordID(RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
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
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown abbreviation.");
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
requires std::is_integral_v<Integer>
void
BitcodeWriter::
emitRecord(
    Integer Value, RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID]);
    if constexpr(sizeof(Integer) == 4)
    {
        MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer32Abbrev);
    }
#if 0
    else if constexpr(sizeof(Integer) == 2)
    {
        MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer16Abbrev);
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
    MRDOX_ASSERT(RecordIDNameMap[ID]);
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &Integer32ArrayAbbrev);
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
    MRDOX_ASSERT(RecordIDNameMap[ID]);
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &SymbolIDsAbbrev);
    if (!prepRecordData(ID, Values.begin() != Values.end()))
        return;
    Record.push_back(Values.size());
    for(auto const& Sym : Values)
        Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

#if 0
// vector<SpecializedMember>
void
BitcodeWriter::
emitRecord(
    std::vector<SpecializedMember> const& list,
    RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID]);
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &SpecializedMemAbbrev);
    if (!prepRecordData(ID, ! list.empty()))
        return;
    for(auto const& mem : list)
    {
        Record.push_back(BitCodeConstants::USRHashSize);
        Record.append(mem.Primary.begin(), mem.Primary.end());
        Record.push_back(BitCodeConstants::USRHashSize);
        Record.append(mem.Specialized.begin(), mem.Specialized.end());
    }
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}
#endif

// SymbolID
void
BitcodeWriter::
emitRecord(
    SymbolID const& Sym,
    RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &SymbolIDAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, Sym != SymbolID::zero))
        return;
    MRDOX_ASSERT(Sym.size() == 20);
    Record.push_back(Sym.size());
    Record.append(Sym.begin(), Sym.end());
    Stream.EmitRecordWithAbbrev(Abbrevs.get(ID), Record);
}

void
BitcodeWriter::
emitRecord(
    llvm::StringRef Str, RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &StringAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, !Str.empty()))
        return;
    MRDOX_ASSERT(Str.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Str.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Str);
}

void
BitcodeWriter::
emitRecord(
    Location const& Loc, RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &LocationAbbrev &&
        "Abbrev type mismatch.");
    if (!prepRecordData(ID, true))
        return;
    // FIXME: Assert that the line number
    // is of the appropriate size.
    Record.push_back(Loc.LineNumber);
    MRDOX_ASSERT(Loc.Filename.size() < (1U << BitCodeConstants::StringLengthSize));
    Record.push_back(Loc.IsFileInRootDir);
    Record.push_back(Loc.Filename.size());
    Stream.EmitRecordWithBlob(Abbrevs.get(ID), Record, Loc.Filename);
}

void
BitcodeWriter::
emitRecord(
    bool Val, RecordID ID)
{
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
    MRDOX_ASSERT(RecordIDNameMap[ID].Abbrev == &BoolAbbrev && "Abbrev type mismatch.");
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
    MRDOX_ASSERT(RecordIDNameMap[ID] && "Unknown RecordID.");
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
    MRDOX_ASSERT(RIDs.size() < (1U << BitCodeConstants::SubblockIDSize));
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
    AnyList<T> const& list)
{
    StreamSubBlockGuard Block(Stream, BI_JAVADOC_LIST_BLOCK_ID);
    emitRecord(T::static_kind, JAVADOC_LIST_KIND);
    for(Javadoc::Node const& node : list)
        emitBlock(node);
}

void
BitcodeWriter::
emitInfoPart(
    Info const& I)
{
    StreamSubBlockGuard Block(Stream, BI_INFO_PART_ID);
    emitRecord(I.id, INFO_PART_ID);
    emitRecord(I.Access, INFO_PART_ACCESS);
    emitRecord(I.Name, INFO_PART_NAME);
    emitRecord(I.Namespace, INFO_PART_PARENTS);
    emitBlock(I.javadoc);
}

void
BitcodeWriter::
emitSymbolPart(
    const Info& I,
    const SymbolInfo& S)
{
    StreamSubBlockGuard Block(Stream, BI_SYMBOL_PART_ID);
    if(S.DefLoc)
        emitRecord(*S.DefLoc, SYMBOL_PART_DEFLOC);
    // VFALCO hack to squelch refs from typedefs
    if(I.Kind != InfoKind::Typedef)
        for(const auto& L : S.Loc)
            emitRecord(L, SYMBOL_PART_LOC);
}

void
BitcodeWriter::
emitBlock(
    BaseInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_BASE_BLOCK_ID);
    emitRecord(I.id, BASE_ID);
    emitRecord(I.Name, BASE_NAME);
    emitRecord(I.Access, BASE_ACCESS);
    emitRecord(I.IsVirtual, BASE_IS_VIRTUAL);
}

void
BitcodeWriter::
emitBlock(
    EnumInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_ENUM_BLOCK_ID);
    emitInfoPart(I);
    emitSymbolPart(I, I);
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
    FieldInfo const& F)
{
    StreamSubBlockGuard Block(Stream, BI_FIELD_BLOCK_ID);
    emitInfoPart(F);
    emitSymbolPart(F, F);
    emitBlock(F.Type);
    emitRecord(F.Name, FIELD_NAME);
    emitRecord(F.Default, FIELD_DEFAULT);
    emitRecord({F.specs.raw}, FIELD_ATTRIBUTES);
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
    emitSymbolPart(I, I);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord({I.specs0.raw, I.specs1.raw}, FUNCTION_BITS);
    emitBlock(I.ReturnType);
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
        emitRecord(J.direction, JAVADOC_PARAM_DIRECTION);
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
        // unknown kind
        MRDOX_UNREACHABLE();
        break;
    }
}

void
BitcodeWriter::
emitBlock(
    NamespaceInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_NAMESPACE_BLOCK_ID);
    emitInfoPart(I);
    emitRecord(I.Members, NAMESPACE_MEMBERS);
    emitRecord(I.Specializations, NAMESPACE_SPECIALIZATIONS);
}

void
BitcodeWriter::
emitBlock(
    RecordInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_RECORD_BLOCK_ID);
    emitInfoPart(I);
    emitSymbolPart(I, I);
    if (I.Template)
        emitBlock(*I.Template);
    emitRecord(I.KeyKind, RECORD_KEY_KIND);
    emitRecord(I.IsTypeDef, RECORD_IS_TYPE_DEF);
    emitRecord({I.specs.raw}, RECORD_BITS);
    for (const auto& B : I.Bases)
        emitBlock(B);
    emitRecord(I.Friends, RECORD_FRIENDS);
    emitRecord(I.Members, RECORD_MEMBERS);
    emitRecord(I.Specializations, RECORD_SPECIALIZATIONS);
}

void
BitcodeWriter::
emitBlock(
    const SpecializationInfo& I)
{
    StreamSubBlockGuard Block(Stream, BI_SPECIALIZATION_BLOCK_ID);
    emitInfoPart(I);
    emitRecord(I.Primary, SPECIALIZATION_PRIMARY);
    for(const auto& targ : I.Args)
        emitBlock(targ);
    std::vector<SymbolID> members;
    members.reserve(I.Members.size() * 2);
    for(const auto& mem : I.Members)
    {
        members.emplace_back(mem.Primary);
        members.emplace_back(mem.Specialized);
    }
    emitRecord(members, SPECIALIZATION_MEMBERS);
}

void
BitcodeWriter::
emitBlock(
    const TemplateInfo& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_BLOCK_ID);
    if(T.Primary)
        emitRecord(*T.Primary, TEMPLATE_PRIMARY_USR);
    for(const auto& targ : T.Args)
        emitBlock(targ);
    for(const auto& tparam : T.Params)
        emitBlock(tparam);
}

void
BitcodeWriter::
emitBlock(
    const TParam& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_PARAM_BLOCK_ID);
    emitRecord(T.Kind, TEMPLATE_PARAM_KIND);
    emitRecord(T.Name, TEMPLATE_PARAM_NAME);
    emitRecord(T.IsParameterPack, TEMPLATE_PARAM_IS_PACK);
    switch(T.Kind)
    {
    case TParamKind::Type:
    {
        const auto& info = T.get<TypeTParam>();
        if(info.Default)
            emitBlock(*info.Default);
        break;
    }
    case TParamKind::NonType:
    {
        const auto& info = T.get<NonTypeTParam>();
        emitBlock(info.Type);
        if(info.Default)
            emitRecord(*info.Default, TEMPLATE_PARAM_DEFAULT);
        break;
    }
    case TParamKind::Template:
    {
        const auto& info = T.get<TemplateTParam>();
        for(const auto& P : info.Params)
            emitBlock(P);
        if(info.Default)
            emitRecord(*info.Default, TEMPLATE_PARAM_DEFAULT);
        break;
    }
    default:
        break;
    }
}

void
BitcodeWriter::
emitBlock(
    const TArg& T)
{
    StreamSubBlockGuard Block(Stream, BI_TEMPLATE_ARG_BLOCK_ID);
    emitRecord(T.Value, TEMPLATE_ARG_VALUE);
}

void
BitcodeWriter::
emitBlock(
    TypedefInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_TYPEDEF_BLOCK_ID);
    emitInfoPart(I);
    emitSymbolPart(I, I);
    emitRecord(I.IsUsing, TYPEDEF_IS_USING);
    emitBlock(I.Underlying);
    if(I.Template)
        emitBlock(*I.Template);
}

void
BitcodeWriter::
emitBlock(
    TypeInfo const& T)
{
    if (T.id == SymbolID::zero && T.Name.empty())
        return;
    StreamSubBlockGuard Block(Stream, BI_TYPE_BLOCK_ID);
    emitRecord(T.id, TYPE_ID);
    emitRecord(T.Name, TYPE_NAME);
}

void
BitcodeWriter::
emitBlock(
    VarInfo const& I)
{
    StreamSubBlockGuard Block(Stream, BI_VARIABLE_BLOCK_ID);
    emitInfoPart(I);
    emitSymbolPart(I, I);
    if(I.Template)
        emitBlock(*I.Template);
    emitBlock(I.Type);
    emitRecord({I.specs.raw}, VARIABLE_BITS);
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
