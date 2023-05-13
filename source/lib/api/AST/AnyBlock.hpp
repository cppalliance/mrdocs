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

#ifndef MRDOX_API_AST_ANYBLOCK_HPP
#define MRDOX_API_AST_ANYBLOCK_HPP

#include "BitcodeReader.hpp"
#include "AnyNodeList.hpp"
#include "DecodeRecord.hpp"
#include "api/Support/Debug.hpp"

namespace clang {
namespace mrdox {

//------------------------------------------------

struct BitcodeReader::AnyBlock
{
    virtual ~AnyBlock() = default;

    virtual
    llvm::Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob)
    {
        return makeError("unexpected record with ID=", ID);
    }

    virtual
    llvm::Error
    readSubBlock(unsigned ID)
    {
        return makeError("unexpected sub-block with ID=", ID);
    }

    llvm::Error
    makeWrongFieldError(FieldId F)
    {
        return makeError("unexpected FieldId=",
            static_cast<std::underlying_type_t<FieldId>>(F));
    }
};

//------------------------------------------------

class VersionBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;

public:
    unsigned V;

    explicit
    VersionBlock(
        BitcodeReader& br) noexcept
        : br_(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case VERSION:
            if(auto Err = decodeRecord(R, V, Blob))
                return Err;
            if(V != BitcodeVersion)
                return makeError("wrong ID for Version");
            return llvm::Error::success();
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class ReferenceBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;

public:
    Reference I;
    FieldId F;

    explicit
    ReferenceBlock(
        BitcodeReader& br) noexcept
        : br_(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case REFERENCE_USR:
            return decodeRecord(R, I.id, Blob);
        case REFERENCE_NAME:
            return decodeRecord(R, I.Name, Blob);
        case REFERENCE_TYPE:
            return decodeRecord(R, I.RefType, Blob);
        case REFERENCE_FIELD:
            return decodeRecord(R, F, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

template<class Container>
class ReferencesBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    Container& C_;
    FieldId F_;
    Reference I;

public:
    ReferencesBlock(
        Container& C,
        BitcodeReader& br) noexcept
        : br_(br)
        , C_(C)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        ReferenceBlock B(br_);
        if(auto Err = br_.readBlock(B, ID))
            return Err;
        //if(B.F != F_)
            //return makeWrongFieldError(B.F);
        C_.emplace_back(B.I);
        return llvm::Error::success();
    }
};

//------------------------------------------------

/** An AnyNodeList
*/
class JavadocNodesBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;

public:
    AnyNodeList J;

    explicit
    JavadocNodesBlock(
        AnyNodeList*& stack,
        BitcodeReader& br) noexcept
        : br_(br)
        , J(stack)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch (ID)
        {
        case JAVADOC_LIST_KIND:
        {
            Javadoc::Kind kind;
            if(auto Err = decodeRecord(R, kind, Blob))
                return Err;
            return J.setKind(kind);
        }
        case JAVADOC_NODE_KIND:
        {
            Javadoc::Kind kind;
            if(auto Err = decodeRecord(R, kind, Blob))
                return Err;
            return J.getNodes().appendChild(kind);
        }
        case JAVADOC_PARAM_DIRECTION:
        {
            Javadoc::ParamDirection direction;
            if(auto Err = decodeRecord(R, direction, Blob))
                return Err;
            return J.getNodes().setDirection(direction);
        }
        case JAVADOC_NODE_STRING:
        {
            return J.getNodes().setString(Blob);
        }
        case JAVADOC_NODE_STYLE:
        {
            Javadoc::Style style;
            if(auto Err = decodeRecord(R, style, Blob))
                return Err;
            return J.getNodes().setStyle(style);
        }
        case JAVADOC_NODE_ADMONISH:
        {
            Javadoc::Admonish admonish;
            if(auto Err = decodeRecord(R, admonish, Blob))
                return Err;
            return J.getNodes().setAdmonish(admonish);
        }
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_JAVADOC_NODE_BLOCK_ID:
        {
            return br_.readBlock(*this, ID);
        }
        case BI_JAVADOC_LIST_BLOCK_ID:
        {
            JavadocNodesBlock B(J.stack(), br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            if(auto Err = B.J.spliceIntoParent())
                return Err;
            return llvm::Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class JavadocBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    llvm::Optional<Javadoc>& I_;
    AnyNodeList* stack_ = nullptr;
    AnyNodeList J_;

public:
    JavadocBlock(
        llvm::Optional<Javadoc>& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
        , J_(stack_)
    {
        I_.emplace();
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_JAVADOC_LIST_BLOCK_ID:
        {
            JavadocNodesBlock B(stack_, br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            if(auto Err = B.J.spliceInto(I_->getBlocks()))
                return Err;
            return llvm::Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class InfoPartBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    Info& I;

public:
    InfoPartBlock(
        Info& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I(I)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case INFO_PART_ID:
            return decodeRecord(R, I.id, Blob);
        case INFO_PART_NAME:
            return decodeRecord(R, I.Name, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_REFERENCE_BLOCK_ID:
        {
            ReferenceBlock B(br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            switch(B.F)
            {
            case FieldId::F_namespace:
                I.Namespace.emplace_back(std::move(B.I));
                break;
            default:
                return AnyBlock::readSubBlock(ID);
            }
            return llvm::Error::success();
        }
        case BI_JAVADOC_BLOCK_ID:
        {
            JavadocBlock B(I.javadoc, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return AnyBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class SymbolPartBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    SymbolInfo& I;

public:
    SymbolPartBlock(
        SymbolInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I(I)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case SYMBOL_PART_LOCDEF:
            return decodeRecord(R, I.DefLoc, Blob);
        case SYMBOL_PART_LOC:
            return decodeRecord(R, I.Loc, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class TypeBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    TypeInfo& I_;

public:
    FieldId F;

    TypeBlock(
        TypeInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_REFERENCE_BLOCK_ID:
        {
            ReferenceBlock B(br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            F = B.F;
            I_.Type = std::move(B.I);
            return llvm::Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class FieldTypeBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    FieldTypeInfo& I_;

public:
    FieldId F;

    FieldTypeBlock(
        FieldTypeInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    llvm::Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FIELD_TYPE_NAME:
            return decodeRecord(R, I_.Name, Blob);
        case FIELD_DEFAULT_VALUE:
            return decodeRecord(R, I_.DefaultValue, Blob);
        case FIELD_ATTRIBUTES:
            return decodeRecord(R, {&I_.Flags.raw}, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_REFERENCE_BLOCK_ID:
        {
            ReferenceBlock B(br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            F = B.F;
            I_.Type = std::move(B.I);
            return llvm::Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class MemberTypeBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    MemberTypeInfo& I_;

public:
    MemberTypeBlock(
        MemberTypeInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    llvm::Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case MEMBER_TYPE_NAME:
            return decodeRecord(R, I_.Name, Blob);
        case MEMBER_TYPE_ACCESS:
            return decodeRecord(R, I_.access, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_REFERENCE_BLOCK_ID:
        {
            ReferenceBlock B(br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            I_.Type = std::move(B.I);
            return llvm::Error::success();
        }
        case BI_FIELD_TYPE_BLOCK_ID:
        {
            FieldTypeBlock B(I_, br_);
            return br_.readBlock(B, ID);
        }
        case BI_JAVADOC_BLOCK_ID:
        {
            JavadocBlock B(I_.javadoc, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class BaseBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    std::vector<BaseInfo>& v_;

public:
    explicit
    BaseBlock(
        std::vector<BaseInfo>& v,
        BitcodeReader& br) noexcept
        : br_(br)
        , v_(v)
    {
        v_.emplace_back();
    }

    llvm::Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case BASE_ID:
            return decodeRecord(R, v_.back().id, Blob);
        case BASE_NAME:
            return decodeRecord(R, v_.back().Name, Blob);
        case BASE_ACCESS:
            return decodeRecord(R, v_.back().access, Blob);
        case BASE_IS_VIRTUAL:
            return decodeRecord(R, v_.back().IsVirtual, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class TemplateSpecializationBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    llvm::Optional<TemplateSpecializationInfo>& I_;

public:
    TemplateSpecializationBlock(
        llvm::Optional<TemplateSpecializationInfo>& I,
        BitcodeReader& br)
        : br_(br)
        , I_(I)
    {
        I_.emplace();
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TEMPLATE_SPECIALIZATION_OF:
            return decodeRecord(R, I_->SpecializationOf, Blob);
        case TEMPLATE_PARAM_CONTENTS:
            return decodeRecord(R, I_->Params.back().Contents, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TEMPLATE_PARAM_BLOCK_ID:
        {
            I_->Params.emplace_back();
            if(auto Err = br_.readBlock(*this, ID))
                return Err;
            return llvm::Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class TemplateBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    llvm::Optional<TemplateInfo>& I_;

public:
    explicit
    TemplateBlock(
        llvm::Optional<TemplateInfo>& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
        I_.emplace();
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TEMPLATE_PARAM_CONTENTS:
            return decodeRecord(R, I_->Params.back().Contents, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TEMPLATE_PARAM_BLOCK_ID:
        {
            I_->Params.emplace_back();
            if(auto Err = br_.readBlock(*this, ID))
                return Err;
            return llvm::Error::success();
        }
        case BI_TEMPLATE_SPECIALIZATION_BLOCK_ID:
        {
            TemplateSpecializationBlock B(I_->Specialization, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

template<class T>
class TopLevelBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;

public:
    std::unique_ptr<T> I;

    explicit
    TopLevelBlock(
        BitcodeReader& br)
        : br_(br)
        , I(std::make_unique<T>())
    {
    }

    llvm::Error
    insertChild(
        Reference&& R, FieldId Id)
    {
        switch(Id)
        {
        case FieldId::F_child_namespace:
        {
            // Record can't have namespace
            if constexpr(
                std::derived_from<T, NamespaceInfo>)
            {
                I->Children.Namespaces.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        case FieldId::F_child_record:
        {
            if constexpr(
                std::derived_from<T, NamespaceInfo> ||
                std::derived_from<T, RecordInfo>)
            {
                I->Children.Records.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        case FieldId::F_child_function:
        {
            if constexpr(
                std::derived_from<T, NamespaceInfo> ||
                std::derived_from<T, RecordInfo>)
            {
                I->Children.Functions.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        case FieldId::F_child_typedef:
        {
            if constexpr(
                std::derived_from<T, NamespaceInfo> ||
                std::derived_from<T, RecordInfo>)
            {
                I->Children.Typedefs.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        case FieldId::F_child_enum:
        {
            if constexpr(
                std::derived_from<T, NamespaceInfo> ||
                std::derived_from<T, RecordInfo>)
            {
                I->Children.Enums.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        case FieldId::F_child_variable:
        {
            if constexpr(
                std::derived_from<T, NamespaceInfo> ||
                std::derived_from<T, RecordInfo>)
            {
                I->Children.Vars.emplace_back(std::move(R));
                return llvm::Error::success();
            }
            break;
        }
        default:
            return makeWrongFieldError(Id);
        }
        return makeError("unknown type");
    }

    llvm::Error readChild(Scope& I, unsigned ID);
    llvm::Error readSubBlock(unsigned ID) override;
};

//------------------------------------------------

class NamespaceBlock
    : public TopLevelBlock<NamespaceInfo>
{

public:
    explicit
    NamespaceBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }
};

//------------------------------------------------

class RecordBlock
    : public TopLevelBlock<RecordInfo>
{
public:
    explicit
    RecordBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case RECORD_TAG_TYPE:
            return decodeRecord(R, I->TagType, Blob);
        case RECORD_IS_TYPE_DEF:
            return decodeRecord(R, I->IsTypeDef, Blob);
        case RECORD_BITS:
            return decodeRecord(R, {&I->specs.raw}, Blob);
        case RECORD_FRIENDS:
            return decodeRecord(R, I->Friends, Blob);
        case RECORD_ENUMS:
            return decodeRecord(R, I->Children_.Enums, Blob);
        case RECORD_FUNCTIONS:
            return decodeRecord(R, I->Children_.Functions, Blob);
        case RECORD_RECORDS:
            return decodeRecord(R, I->Children_.Records, Blob);
        case RECORD_TYPES:
            return decodeRecord(R, I->Children_.Types, Blob);
        case RECORD_VARS:
            return decodeRecord(R, I->Children_.Vars, Blob);

        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_MEMBER_TYPE_BLOCK_ID:
        {
            MemberTypeBlock B(I->Members.emplace_back(), br_);
            return br_.readBlock(B, ID);
        }
        case BI_BASE_BLOCK_ID:
        {
            BaseBlock B(I->Bases, br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            TemplateBlock B(I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class FunctionBlock
    : public TopLevelBlock<FunctionInfo>
{
public:
    explicit
    FunctionBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FUNCTION_BITS:
            return decodeRecord(R, {&I->specs0.raw, &I->specs1.raw}, Blob);
        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->ReturnType, br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return llvm::Error::success();
        }
        case BI_FIELD_TYPE_BLOCK_ID:
        {
            FieldTypeBlock B(I->Params.emplace_back(), br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return llvm::Error::success();
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            TemplateBlock B(I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class TypedefBlock
    : public TopLevelBlock<TypedefInfo>
{

public:
    explicit
    TypedefBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TYPEDEF_IS_USING:
            return decodeRecord(R, I->IsUsing, Blob);
        default:
            break;
        }
        return TopLevelBlock::parseRecord(R, ID, Blob);
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->Underlying, br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return llvm::Error::success();
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class EnumValueBlock : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    EnumValueInfo& I_;

public:
    EnumValueBlock(
        EnumValueInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case ENUM_VALUE_NAME:
            return decodeRecord(R, I_.Name, Blob);
        case ENUM_VALUE_VALUE:
            return decodeRecord(R, I_.Value, Blob);
        case ENUM_VALUE_EXPR:
            return decodeRecord(R, I_.ValueExpr, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

class EnumBlock
    : public TopLevelBlock<EnumInfo>
{
public:
    explicit
    EnumBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case ENUM_SCOPED:
            return decodeRecord(R, I->Scoped, Blob);
        default:
            break;
        }
        return TopLevelBlock::parseRecord(R, ID, Blob);
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            I->BaseType.emplace();
            TypeBlock B(*I->BaseType, br_);
            return br_.readBlock(B, ID);
        }
        case BI_ENUM_VALUE_BLOCK_ID:
        {
            I->Members.emplace_back();
            EnumValueBlock B(I->Members.back(), br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            return llvm::Error::success();
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class VarBlock
    : public TopLevelBlock<VarInfo>
{

public:
    explicit
    VarBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    llvm::Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case VARIABLE_BITS:
            return decodeRecord(R, {&I->specs.raw}, Blob);
        default:
            break;
        }
        return TopLevelBlock::parseRecord(R, ID, Blob);
    }

    llvm::Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(*I, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

template<class T>
llvm::Error
TopLevelBlock<T>::
readChild(
    Scope& I, unsigned ID)
{
    ReferenceBlock B(br_);
    if(auto Err = br_.readBlock(B, ID))
        return Err;
    switch(B.F)
    {
    case FieldId::F_child_namespace:
        I.Namespaces.emplace_back(B.I);
        break;
    case FieldId::F_child_record:
        I.Records.emplace_back(B.I);
        break;
    case FieldId::F_child_function:
        I.Functions.emplace_back(B.I);
        break;
    case FieldId::F_child_typedef:
        I.Typedefs.emplace_back(B.I);
        break;
    case FieldId::F_child_enum:
        I.Enums.emplace_back(B.I);
        break;
    case FieldId::F_child_variable:
        I.Vars.emplace_back(B.I);
        break;
    default:
        return makeWrongFieldError(B.F);
    }
    return llvm::Error::success();
}

template<class T>
llvm::Error
TopLevelBlock<T>::
readSubBlock(
    unsigned ID)
{
    switch(ID)
    {
    case BI_INFO_PART_ID:
    {
        if constexpr(std::derived_from<T, Info>)
        {
            InfoPartBlock B(*I.get(), br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            return llvm::Error::success();
        }
        break;
    }
    case BI_SYMBOL_PART_ID:
    {
        if constexpr(std::derived_from<T, SymbolInfo>)
        {
            SymbolPartBlock B(*I.get(), br_);
            if(auto Err = br_.readBlock(B, ID))
                return Err;
            return llvm::Error::success();
        }
        break;
    }
    case BI_REFERENCE_BLOCK_ID:
    {
        if constexpr(std::derived_from<T, NamespaceInfo>)
        {
            return readChild(I->Children, ID);
        }
        break;
    }
    default:
        break;
    }
    return AnyBlock::readSubBlock(ID);
}

} // mrdox
} // clang

#endif
