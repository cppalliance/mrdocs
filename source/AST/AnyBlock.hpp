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

#ifndef MRDOX_TOOL_AST_ANYBLOCK_HPP
#define MRDOX_TOOL_AST_ANYBLOCK_HPP

#include "BitcodeReader.hpp"
#include "AnyNodeList.hpp"
#include "DecodeRecord.hpp"
#include "Support/Debug.hpp"
#include "Support/Error.hpp"

namespace clang {
namespace mrdox {

//------------------------------------------------

struct BitcodeReader::AnyBlock
{
    virtual ~AnyBlock() = default;

    virtual
    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob)
    {
        return Error("unexpected record with ID={}", ID);
    }

    virtual
    Error
    readSubBlock(unsigned ID)
    {
        return Error("unexpected sub-block with ID={}", ID);
    }

    Error
    makeWrongFieldError(FieldId F)
    {
        return Error("unexpected FieldId");
            //static_cast<std::underlying_type_t<FieldId>>(F));
    }
};

//------------------------------------------------

class VersionBlock
    : public BitcodeReader::AnyBlock
{
public:
    unsigned V;

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case VERSION:
            if(auto err = decodeRecord(R, V, Blob))
                return err;
            if(V != BitcodeVersion)
                return Error("wrong ID for Version");
            return Error::success();
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case REFERENCE_USR:
            return decodeRecord(R, I.id, Blob);
        case REFERENCE_NAME:
            return decodeRecord(R, I.Name, Blob);
        case REFERENCE_KIND:
            return decodeRecord(R, I.RefKind, Blob);
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        ReferenceBlock B(br_);
        if(auto err = br_.readBlock(B, ID))
            return err;
        //if(B.F != F_)
            //return makeWrongFieldError(B.F);
        C_.emplace_back(B.I);
        return Error::success();
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch (ID)
        {
        case JAVADOC_LIST_KIND:
        {
            Javadoc::Kind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            return J.setKind(kind);
        }
        case JAVADOC_NODE_KIND:
        {
            Javadoc::Kind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            return J.getNodes().appendChild(kind);
        }
        case JAVADOC_PARAM_DIRECTION:
        {
            Javadoc::ParamDirection direction =
                Javadoc::ParamDirection::none;
            if(auto err = decodeRecord(R, direction, Blob))
                return err;
            return J.getNodes().setDirection(direction);
        }
        case JAVADOC_NODE_STRING:
        {
            return J.getNodes().setString(Blob);
        }
        case JAVADOC_NODE_STYLE:
        {
            Javadoc::Style style =
                Javadoc::Style::none;
            if(auto err = decodeRecord(R, style, Blob))
                return err;
            return J.getNodes().setStyle(style);
        }
        case JAVADOC_NODE_ADMONISH:
        {
            Javadoc::Admonish admonish =
                Javadoc::Admonish::none;
            if(auto err = decodeRecord(R, admonish, Blob))
                return err;
            return J.getNodes().setAdmonish(admonish);
        }
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
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
            if(auto err = br_.readBlock(B, ID))
                return err;
            if(auto err = B.J.spliceIntoParent())
                return err;
            return Error::success();
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
    std::unique_ptr<Javadoc>& I_;
    AnyNodeList* stack_ = nullptr;
    AnyNodeList J_;

public:
    JavadocBlock(
        std::unique_ptr<Javadoc>& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
        , J_(stack_)
    {
        I_ = std::make_unique<Javadoc>();
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_JAVADOC_LIST_BLOCK_ID:
        {
            JavadocNodesBlock B(stack_, br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            if(auto err = B.J.spliceInto(I_->getBlocks()))
                return err;
            return Error::success();
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case INFO_PART_ID:
            return decodeRecord(R, I.id, Blob);
        case INFO_PART_NAME:
            return decodeRecord(R, I.Name, Blob);
        case INFO_PART_PARENTS:
            return decodeRecord(R, I.Namespace, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case SYMBOL_PART_DEFLOC:
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

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_REFERENCE_BLOCK_ID:
        {
            ReferenceBlock B(br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            F = B.F;
            static_cast<Reference&>(I_) =
                std::move(B.I);
            return Error::success();
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
    std::vector<BaseInfo>& v_;

public:
    explicit
    BaseBlock(
        std::vector<BaseInfo>& v) noexcept
        : v_(v)
    {
        v_.emplace_back();
    }

    Error
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

class TemplateArgBlock
    : public BitcodeReader::AnyBlock
{
    TArg& I_;

public:
    TemplateArgBlock(
        TArg& I) noexcept
        : I_(I)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TEMPLATE_ARG_VALUE:
            return decodeRecord(R, I_.Value, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class TemplateParamBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    TParam& I_;

public:
    TemplateParamBlock(
        TParam& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TEMPLATE_PARAM_NAME:
        {
            return decodeRecord(R, I_.Name, Blob);
        }
        case TEMPLATE_PARAM_IS_PACK:
        {
            return decodeRecord(R, I_.IsParameterPack, Blob);
        }
        case TEMPLATE_PARAM_KIND:
        {
            TParamKind kind = TParamKind::None;
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            switch(kind)
            {
            case TParamKind::Type:
                I_.emplace<TypeTParam>();
                break;
            case TParamKind::NonType:
                I_.emplace<NonTypeTParam>();
                break;
            case TParamKind::Template:
                I_.emplace<TemplateTParam>();
                break;
            default:
                return Error("invalid template parameter kind");
            }
            return Error::success();
        }
        case TEMPLATE_PARAM_DEFAULT:
        {
            switch(I_.Kind)
            {
            case TParamKind::NonType:
                return decodeRecord(R,
                    I_.get<NonTypeTParam>().Default.emplace(), Blob);
            case TParamKind::Template:
                return decodeRecord(R,
                    I_.get<TemplateTParam>().Default.emplace(), Blob);
            default:
                return Error("invalid template parameter kind");
            }
        }

        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TEMPLATE_PARAM_BLOCK_ID:
        {
            if(I_.Kind != TParamKind::Template)
                return Error("only TemplateTParam may have template parameters");
            TemplateParamBlock P(I_.get<TemplateTParam>().Params.emplace_back(), br_);
            if(auto err = br_.readBlock(P, ID))
                return err;
            return Error::success();
        }
        case BI_TYPE_BLOCK_ID:
        {
            TypeInfo* t = nullptr;
            switch(I_.Kind)
            {
            case TParamKind::Type:
                t = &I_.get<TypeTParam>().Default.emplace();
                break;
            case TParamKind::NonType:
                t = &I_.get<NonTypeTParam>().Type;
                break;
            default:
                return Error("invalid TypeInfo block in TParam");
            }
            TypeBlock B(*t, br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            // KRYSTIAN NOTE: is this check correct?
            // copied from a function with TypeInfo sub-block
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return Error::success();
        }
        default:
            break;
        }
        return AnyBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class TemplateBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    TemplateInfo& I_;

public:
    explicit
    TemplateBlock(
        TemplateInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case TEMPLATE_PRIMARY_USR:
            return decodeRecord(R, I_.Primary.emplace(), Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TEMPLATE_ARG_BLOCK_ID:
        {
            TemplateArgBlock A(
                I_.Args.emplace_back());
            if(auto err = br_.readBlock(A, ID))
                return err;
            return Error::success();
        }
        case BI_TEMPLATE_PARAM_BLOCK_ID:
        {
            TemplateParamBlock P(
                I_.Params.emplace_back(), br_);
            if(auto err = br_.readBlock(P, ID))
                return err;
            return Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class FunctionParamBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    Param& I_;

public:
    FunctionParamBlock(
        Param& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FUNCTION_PARAM_NAME:
        {
            return decodeRecord(R, I_.Name, Blob);
        }
        case FUNCTION_PARAM_DEFAULT:
        {
            return decodeRecord(R, I_.Default, Blob);
        }
        /*
        case FUNCTION_PARAM_IS_PACK:
        {
            return decodeRecord(R, I_.IsParameterPack, Blob);
        }
        */
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I_.Type, br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            // KRYSTIAN NOTE: is this check correct?
            // copied from a function with TypeInfo sub-block
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return Error::success();
        }
        default:
            break;
        }
        return AnyBlock::readSubBlock(ID);
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

    #if 0
    Error
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
                return Error::success();
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
                return Error::success();
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
                return Error::success();
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
                return Error::success();
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
                return Error::success();
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
                return Error::success();
            }
            break;
        }
        // KRYSTIAN FIXME: i am 90% sure that this isn't needed.
        // we don't use Scope for storing Record members
        case FieldId::F_child_field:
        {
            // Namespace can't have fields
            if constexpr(
                std::derived_from<T, RecordInfo>)
            {
                Assert("RecordInfo doesn't use Scope");
                return Error::success();
            }
            break;
        }
        default:
            return makeWrongFieldError(Id);
        }
        return Error("unknown type");
    }
    #endif

    Error readChild(Scope& I, unsigned ID);
    Error readSubBlock(unsigned ID) override;
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

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case RECORD_KEY_KIND:
            return decodeRecord(R, I->KeyKind, Blob);
        case RECORD_IS_TYPE_DEF:
            return decodeRecord(R, I->IsTypeDef, Blob);
        case RECORD_BITS:
            return decodeRecord(R, {&I->specs.raw}, Blob);
        case RECORD_FRIENDS:
            return decodeRecord(R, I->Friends, Blob);
        case RECORD_ENUMS:
            return decodeRecord(R, I->Members.Enums, Blob);
        case RECORD_FUNCTIONS:
            return decodeRecord(R, I->Members.Functions, Blob);
        case RECORD_RECORDS:
            return decodeRecord(R, I->Members.Records, Blob);
        case RECORD_TYPES:
            return decodeRecord(R, I->Members.Types, Blob);
        case RECORD_FIELDS:
            return decodeRecord(R, I->Members.Fields, Blob);
        case RECORD_VARS:
            return decodeRecord(R, I->Members.Vars, Blob);
        case RECORD_SPECIALIZATIONS:
            return decodeRecord(R, I->Members.Specializations, Blob);
        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_BASE_BLOCK_ID:
        {
            BaseBlock B(I->Bases);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
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

    Error
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

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->ReturnType, br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return Error::success();
        }
        case BI_FUNCTION_PARAM_BLOCK_ID:
        {
            FunctionParamBlock B(I->Params.emplace_back(), br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
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

    Error
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

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->Underlying, br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            switch(B.F)
            {
            case FieldId::F_type:
                break;
            default:
                return makeWrongFieldError(B.F);
            }
            return Error::success();
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
            return br_.readBlock(B, ID);
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
    EnumValueInfo& I_;

public:
    EnumValueBlock(
        EnumValueInfo& I) noexcept
        : I_(I)
    {
    }

    Error
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

    Error
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

    Error
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
            EnumValueBlock B(I->Members.back());
            if(auto err = br_.readBlock(B, ID))
                return err;
            return Error::success();
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

    Error
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

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->Type, br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class FieldBlock
    : public TopLevelBlock<FieldInfo>
{
public:
    explicit
    FieldBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FIELD_NAME:
            return decodeRecord(R, I->Name, Blob);
        case FIELD_DEFAULT:
            return decodeRecord(R, I->Default, Blob);
        case FIELD_ATTRIBUTES:
            return decodeRecord(R, {&I->specs.raw}, Blob);
        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TYPE_BLOCK_ID:
        {
            TypeBlock B(I->Type, br_);
            return br_.readBlock(B, ID);
        }
        default:
            break;
        }
        return TopLevelBlock::readSubBlock(ID);
    }
};

//------------------------------------------------

class SpecializationBlock
    : public TopLevelBlock<SpecializationInfo>
{
public:
    explicit
    SpecializationBlock(
        BitcodeReader& br)
        : TopLevelBlock(br)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case SPECIALIZATION_PRIMARY:
            return decodeRecord(R, I->Primary, Blob);
        case SPECIALIZATION_MEMBERS:
        {
            std::vector<SymbolID> members;
            if(auto err = decodeRecord(R, members, Blob))
                return err;
            for(std::size_t i = 0; i < members.size(); i += 2)
                I->Members.emplace_back(members[i + 0], members[i + 1]);
            return Error::success();

        }
        default:
            break;
        }
        return TopLevelBlock::parseRecord(R, ID, Blob);
    }

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_TEMPLATE_ARG_BLOCK_ID:
        {
            TemplateArgBlock B(I->Args.emplace_back());
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
Error
TopLevelBlock<T>::
readChild(
    Scope& I, unsigned ID)
{
    ReferenceBlock B(br_);
    if(auto err = br_.readBlock(B, ID))
        return err;
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
    case FieldId::F_child_specialization:
        I.Specializations.emplace_back(B.I);
        break;
    default:
        return makeWrongFieldError(B.F);
    }
    return Error::success();
}

template<class T>
Error
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
            if(auto err = br_.readBlock(B, ID))
                return err;
            return Error::success();
        }
        break;
    }
    case BI_SYMBOL_PART_ID:
    {
        if constexpr(std::derived_from<T, SymbolInfo>)
        {
            SymbolPartBlock B(*I.get(), br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            return Error::success();
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
