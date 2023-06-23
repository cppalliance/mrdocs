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

/** A doc::List<doc::Node>
*/
class JavadocNodesBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;

public:
    doc::List<doc::Node> nodes;

    explicit
    JavadocNodesBlock(
        BitcodeReader& br) noexcept
        : br_(br)
    {
    }

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch (ID)
        {
        case JAVADOC_NODE_ADMONISH:
        {
            doc::Admonish admonish =
                doc::Admonish::none;
            if(auto err = decodeRecord(R, admonish, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::admonition)
                return Error("admonish on wrong kind");
            static_cast<doc::Admonition*>(
                node)->style = admonish;
            return Error::success();
        }

        case JAVADOC_PARAM_DIRECTION:
        {
            doc::ParamDirection direction =
                doc::ParamDirection::none;
            if(auto err = decodeRecord(R, direction, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::param)
                return Error("direction on wrong kind");
            auto param = static_cast<doc::Param*>(node);
            param->direction = direction;
            return Error::success();
        }

        case JAVADOC_NODE_HREF:
        {
            switch(auto node = nodes.back().get();
                node->kind)
            {
            case doc::Kind::link:
                static_cast<doc::Link*>(node)->href = Blob.str();
                return Error::success();
            default:
                return Error("href on wrong kind");
            }
        }

        case JAVADOC_NODE_KIND:
        {
            doc::Kind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            return visit(kind,
                [&]<class T>()
                {
                    if constexpr(! std::is_same_v<T, void>)
                    {
                        nodes.emplace_back(std::make_unique<T>());
                        return Error::success();
                    }
                    else
                    {
                        return Error("unknown doc::Kind");
                    }
                });
        }

        case JAVADOC_NODE_STRING:
        {
            switch(auto node = nodes.back().get();
                node->kind)
            {
            case doc::Kind::heading:
                static_cast<doc::Heading*>(node)->string = Blob.str();
                return Error::success();
            case doc::Kind::text:
            case doc::Kind::styled:
            case doc::Kind::link:
                static_cast<doc::Text*>(
                    node)->string = Blob.str();
                return Error::success();
            case doc::Kind::param:
                static_cast<doc::Param*>(
                    node)->name = Blob.str();
                return Error::success();
            case doc::Kind::tparam:
                static_cast<doc::TParam*>(
                    node)->name = Blob.str();
                return Error::success();
            default:
                return Error("string on wrong kind");
            }
        }

        case JAVADOC_NODE_STYLE:
        {
            doc::Style style =
                doc::Style::none;
            if(auto err = decodeRecord(R, style, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::styled)
                return Error("style on wrong kind");
            static_cast<doc::Styled*>(
                node)->style = style;
            return Error::success();

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
        case BI_JAVADOC_LIST_BLOCK_ID:
        {
            auto node = nodes.back().get();
            if(node->kind == doc::Kind::text ||
                node->kind == doc::Kind::styled)
                return Error("text node cannot have list");

            JavadocNodesBlock B(br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            Javadoc::append(static_cast<
                doc::Block*>(node)->children,
                std::move(B.nodes));
            return Error::success();
        }

        case BI_JAVADOC_NODE_BLOCK_ID:
        {
            return br_.readBlock(*this, ID);
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

public:
    JavadocBlock(
        std::unique_ptr<Javadoc>& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
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
            JavadocNodesBlock B(br_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            Javadoc::append(
                I_->getBlocks(),
                std::move(B.nodes));
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
        case INFO_PART_ACCESS:
            return decodeRecord(R, I.Access, Blob);
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
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class SourceInfoBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    SourceInfo& I;

public:
    SourceInfoBlock(
        SourceInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I(I)
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
        case SOURCE_INFO_DEFLOC:
            return decodeRecord(R, I.DefLoc, Blob);
        case SOURCE_INFO_LOC:
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
    TypeBlock(
        TypeInfo& I,
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
        case TYPE_ID:
            return decodeRecord(R, I_.id, Blob);
        case TYPE_NAME:
            return decodeRecord(R, I_.Name, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class BaseBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    BaseInfo& I_;

public:
    explicit
    BaseBlock(
        BaseInfo& I,
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
        case BASE_ACCESS:
            return decodeRecord(R, I_.Access, Blob);
        case BASE_IS_VIRTUAL:
            return decodeRecord(R, I_.IsVirtual, Blob);
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
            return br_.readBlock(B, ID);
        }
        default:
            return AnyBlock::readSubBlock(ID);
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
            return br_.readBlock(P, ID);
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
            return br_.readBlock(B, ID);
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
            return br_.readBlock(A, ID);
        }
        case BI_TEMPLATE_PARAM_BLOCK_ID:
        {
            TemplateParamBlock P(
                I_.Params.emplace_back(), br_);
            return br_.readBlock(P, ID);
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
            return decodeRecord(R, I_.Name, Blob);
        case FUNCTION_PARAM_DEFAULT:
            return decodeRecord(R, I_.Default, Blob);
        /*
        case FUNCTION_PARAM_IS_PACK:
            return decodeRecord(R, I_.IsParameterPack, Blob);
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

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case NAMESPACE_MEMBERS:
            return decodeRecord(R, I->Members, Blob);
        case NAMESPACE_SPECIALIZATIONS:
            return decodeRecord(R, I->Specializations, Blob);
        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
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
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
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
        case RECORD_MEMBERS:
            return decodeRecord(R, I->Members, Blob);
        case RECORD_SPECIALIZATIONS:
            return decodeRecord(R, I->Specializations, Blob);
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
            BaseBlock B(I->Bases.emplace_back(), br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
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
            return br_.readBlock(B, ID);
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
            return TopLevelBlock::readSubBlock(ID);
        }
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
            TypeBlock B(I->Underlying, br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class EnumValueBlock : public BitcodeReader::AnyBlock
{
    EnumValueInfo& I_;
    BitcodeReader& br_;

public:
    EnumValueBlock(
        EnumValueInfo& I,
        BitcodeReader& br) noexcept
        : I_(I)
        , br_(br)
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

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
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
            I->BaseType.emplace();
            TypeBlock B(*I->BaseType, br_);
            return br_.readBlock(B, ID);
        }
        case BI_ENUM_VALUE_BLOCK_ID:
        {
            I->Members.emplace_back();
            EnumValueBlock B(I->Members.back(), br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class VarBlock
    : public TopLevelBlock<VariableInfo>
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
        case BI_TEMPLATE_BLOCK_ID:
        {
            I->Template = std::make_unique<TemplateInfo>();
            TemplateBlock B(*I->Template, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
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
            return TopLevelBlock::readSubBlock(ID);
        }
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
            return TopLevelBlock::parseRecord(R, ID, Blob);
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
            TemplateArgBlock B(I->Args.emplace_back());
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

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
            return br_.readBlock(B, ID);
        }
        break;
    }
    case BI_SOURCE_INFO_ID:
    {
        if constexpr(std::derived_from<T, SourceInfo>)
        {
            SourceInfoBlock B(*I.get(), br_);
            return br_.readBlock(B, ID);
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
