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

#ifndef MRDOCS_LIB_AST_ANYBLOCK_HPP
#define MRDOCS_LIB_AST_ANYBLOCK_HPP

#include "BitcodeReader.hpp"

namespace clang {
namespace mrdocs {

// bool
inline
Error
decodeRecord(
    Record const& R,
    bool& Field,
    llvm::StringRef Blob)
{
    Field = R[0] != 0;
    return Error::success();
}

// 32 bit integral types
template<class IntTy>
requires std::integral<IntTy>
Error
decodeRecord(
    Record const& R,
    IntTy& v,
    llvm::StringRef Blob)
{
    v = 0;
    if (R[0] > (std::numeric_limits<IntTy>::max)())
        return formatError("integer overflow");
    v = static_cast<IntTy>(R[0]);
    return Error::success();
}

// integral types wider than 32 bits
template<class IntTy>
requires (std::integral<IntTy> &&
    sizeof(IntTy) > 4)
Error
decodeRecord(
    Record const& R,
    IntTy& v,
    llvm::StringRef Blob)
{
    v = static_cast<IntTy>(
        static_cast<std::uint64_t>(R[0]) |
        (static_cast<std::uint64_t>(R[1]) << 32));
    return Error::success();
}

// enumerations
template<class Enum>
requires std::is_enum_v<Enum>
Error
decodeRecord(
    Record const& R,
    Enum& value,
    llvm::StringRef blob)
{
    static_assert(! std::same_as<Enum, InfoKind>);
    std::underlying_type_t<Enum> temp;
    if(auto err = decodeRecord(R, temp, blob))
        return err;
    value = static_cast<Enum>(temp);
    return Error::success();
}

// container of char
template<class Field>
requires std::is_same_v<
    typename Field::value_type, char>
Error
decodeRecord(
    const Record& R,
    Field& f,
    llvm::StringRef blob)
        requires requires
        {
            f.assign(blob.begin(), blob.end());
        }
{
    f.assign(blob.begin(), blob.end());
    return Error::success();
}

// range<SymbolID>
inline
Error
decodeRecord(
    Record const& R,
    std::vector<SymbolID>& f,
    llvm::StringRef blob)
{
    auto src = R.begin();
    auto n = *src++;
    f.resize(n);
    auto* dest = &f[0];
    while(n--)
    {
        *dest++ = SymbolID(src);
        src += BitCodeConstants::USRHashSize;
    }
    return Error::success();
}

inline
Error
decodeRecord(
    const Record& R,
    SymbolID& Field,
    llvm::StringRef Blob)
{
    if (R[0] != BitCodeConstants::USRHashSize)
        return formatError("USR digest size={}", R[0]);

    Field = SymbolID(&R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    Record const& R,
    OptionalLocation& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return formatError("integer value {} too large", R[0]);
    Field.emplace((int)R[0], Blob, (bool)R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    const Record& R,
    std::vector<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return formatError("integer {} is too large", R[0]);
    Field.emplace_back((int)R[0], Blob, (bool)R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    Record const& R,
    std::initializer_list<BitFieldFullValue*> values,
    llvm::StringRef Blob)
{
    auto n = R[0];
    if(n != values.size())
        return formatError("wrong size={} for Bitfields[{}]", n, values.size());

    auto itr = values.begin();
    for(std::size_t i = 0; i < values.size(); ++i)
    {

        auto const v = R[i + 1];
        if(v > (std::numeric_limits<std::uint32_t>::max)())
            return formatError("{} is out of range for Bits", v);
        **itr++ = v;
    }
    return Error::success();
}

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
        return formatError("unexpected record with ID={}", ID);
    }

    virtual
    Error
    readSubBlock(unsigned ID)
    {
        return formatError("unexpected sub-block with ID={}", ID);
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
                return formatError("wrong ID for Version");
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
    Javadoc& jd_;

public:
    doc::List<doc::Node> nodes;

    JavadocNodesBlock(
        BitcodeReader& br,
        Javadoc& jd) noexcept
        : br_(br)
        , jd_(jd)
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
            doc::Admonish admonish = doc::Admonish::none;
            if(auto err = decodeRecord(R, admonish, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::admonition)
                return formatError("admonish on wrong kind");
            static_cast<doc::Admonition*>(node)->admonish = admonish;
            return Error::success();
        }

        case JAVADOC_NODE_PART:
        {
            doc::Parts parts = doc::Parts::all;
            if(auto err = decodeRecord(R, parts, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::copied)
                return formatError("part on wrong kind");
            static_cast<doc::Copied*>(node)->parts = parts;
            return Error::success();
        }

        case JAVADOC_NODE_SYMBOLREF:
        {
            SymbolID id;
            if(auto err = decodeRecord(R, id, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::reference &&
                node->kind != doc::Kind::copied)
                return formatError("reference on wrong kind");
            static_cast<doc::Reference*>(node)->id = id;
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
                return formatError("direction on wrong kind");
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
                return formatError("href on wrong kind");
            }
        }

        case JAVADOC_NODE_KIND:
        {
            doc::Kind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            switch(kind)
            {
            case doc::Kind::admonition:
                nodes.emplace_back(std::make_unique<doc::Admonition>());
                break;
            case doc::Kind::brief:
                nodes.emplace_back(std::make_unique<doc::Brief>());
                break;
            case doc::Kind::code:
                nodes.emplace_back(std::make_unique<doc::Code>());
                break;
            case doc::Kind::heading:
                nodes.emplace_back(std::make_unique<doc::Heading>());
                break;
            case doc::Kind::paragraph:
                nodes.emplace_back(std::make_unique<doc::Paragraph>());
                break;
            case doc::Kind::link:
                nodes.emplace_back(std::make_unique<doc::Link>());
                break;
            case doc::Kind::reference:
                nodes.emplace_back(std::make_unique<doc::Reference>());
                break;
            case doc::Kind::copied:
                nodes.emplace_back(std::make_unique<doc::Copied>());
                break;
            case doc::Kind::list_item:
                nodes.emplace_back(std::make_unique<doc::ListItem>());
                break;
            case doc::Kind::param:
                nodes.emplace_back(std::make_unique<doc::Param>());
                break;
            case doc::Kind::returns:
                nodes.emplace_back(std::make_unique<doc::Returns>());
                break;
            case doc::Kind::styled:
                nodes.emplace_back(std::make_unique<doc::Styled>());
                break;
            case doc::Kind::text:
                nodes.emplace_back(std::make_unique<doc::Text>());
                break;
            case doc::Kind::tparam:
                nodes.emplace_back(std::make_unique<doc::TParam>());
                break;
            default:
                return formatError("unknown doc::Kind");
            }
            return Error::success();
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
            case doc::Kind::reference:
            case doc::Kind::copied:
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
                return formatError("string on wrong kind");
            }
        }

        case JAVADOC_NODE_STYLE:
        {
            doc::Style style = doc::Style::none;
            if(auto err = decodeRecord(R, style, Blob))
                return err;
            auto node = nodes.back().get();
            if(node->kind != doc::Kind::styled)
                return formatError("style on wrong kind");
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
                return formatError("text node cannot have list");

            JavadocNodesBlock B(br_, jd_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            static_cast<doc::Block*>(node)->append(
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
        BitcodeReader& br)
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
            JavadocNodesBlock B(br_, *I_);
            if(auto err = br_.readBlock(B, ID))
                return err;
            I_->append(std::move(B.nodes));
            return Error::success();
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

template<typename InfoTy>
class InfoPartBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    std::unique_ptr<InfoTy>& I_;

public:
    InfoPartBlock(
        std::unique_ptr<InfoTy>& I,
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
        case INFO_PART_ID:
        {
            SymbolID id = SymbolID::invalid;
            if(auto err = decodeRecord(R, id, Blob))
                return err;
            I_ = std::make_unique<InfoTy>(id);
            return Error::success();
        }
        case INFO_PART_ACCESS:
            return decodeRecord(R, I_->Access, Blob);
        case INFO_PART_IMPLICIT:
            return decodeRecord(R, I_->Implicit, Blob);
        case INFO_PART_NAME:
            return decodeRecord(R, I_->Name, Blob);
        case INFO_PART_PARENTS:
            return decodeRecord(R, I_->Namespace, Blob);
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
            JavadocBlock B(I_->javadoc, br_);
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

class ExprBlock
    : public BitcodeReader::AnyBlock
{
private:

protected:
    BitcodeReader& br_;
    ExprInfo& I_;
    void(*on_value)(
        ExprInfo&,
        std::uint64_t) = nullptr;

public:
    ExprBlock(
        ExprInfo& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
    {
    }

    template<typename T>
    ExprBlock(
        ConstantExprInfo<T>& I,
        BitcodeReader& br) noexcept
        : br_(br)
        , I_(I)
        , on_value([](
            ExprInfo& expr, std::uint64_t val)
            {
                static_cast<ConstantExprInfo<T>&>(
                    expr).Value.emplace(val);
            })
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
        case EXPR_WRITTEN:
            return decodeRecord(R, I_.Written, Blob);
        case EXPR_VALUE:
        {
            if(! on_value)
                return Error("EXPR_VALUE for expression without value");
            std::uint64_t value = 0;
            if(auto err = decodeRecord(R, value, Blob))
                return err;
            on_value(I_, value);
            return Error::success();

        }
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }
};

//------------------------------------------------

class TypeInfoBlock
    : public BitcodeReader::AnyBlock
{
protected:
    BitcodeReader& br_;
    std::unique_ptr<TypeInfo>& I_;

public:
    TypeInfoBlock(
        std::unique_ptr<TypeInfo>& I,
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
        case TYPEINFO_KIND:
        {
            TypeKind k{};
            if(auto err = decodeRecord(R, k, Blob))
                return err;
            switch(k)
            {
            case TypeKind::Builtin:
                I_ = std::make_unique<BuiltinTypeInfo>();
                break;
            case TypeKind::Tag:
                I_ = std::make_unique<TagTypeInfo>();
                break;
            case TypeKind::Specialization:
                I_ = std::make_unique<SpecializationTypeInfo>();
                break;
            case TypeKind::LValueReference:
                I_ = std::make_unique<LValueReferenceTypeInfo>();
                break;
            case TypeKind::RValueReference:
                I_ = std::make_unique<RValueReferenceTypeInfo>();
                break;
            case TypeKind::Pointer:
                I_ = std::make_unique<PointerTypeInfo>();
                break;
            case TypeKind::MemberPointer:
                I_ = std::make_unique<MemberPointerTypeInfo>();
                break;
            case TypeKind::Array:
                I_ = std::make_unique<ArrayTypeInfo>();
                break;
            case TypeKind::Function:
                I_ = std::make_unique<FunctionTypeInfo>();
                break;
            default:
                return Error("invalid TypeInfo kind");
            }
            return Error::success();
        }
        case TYPEINFO_IS_PACK:
            if(auto err = decodeRecord(R, I_->IsPackExpansion, Blob))
                return err;
            return Error::success();
        case TYPEINFO_ID:
            return visit(*I_, [&]<typename T>(T& t)
                {
                    if constexpr(requires { t.id; })
                        return decodeRecord(R, t.id, Blob);
                    else
                        return Error("wrong TypeInfo kind");
                });
        case TYPEINFO_NAME:
            return visit(*I_, [&]<typename T>(T& t)
                {
                    if constexpr(requires { t.Name; })
                        return decodeRecord(R, t.Name, Blob);
                    else
                        return Error("wrong TypeInfo kind");
                });
        case TYPEINFO_CVQUAL:
            return visit(*I_, [&]<typename T>(T& t)
                {
                    if constexpr(requires { t.CVQualifiers; })
                        return decodeRecord(R, t.CVQualifiers, Blob);
                    else
                        return Error("wrong TypeInfo kind");
                });
        case TYPEINFO_REFQUAL:
            if(! I_->isFunction())
                return Error("wrong TypeInfo kind");
            return decodeRecord(R, static_cast<
                FunctionTypeInfo&>(*I_).RefQualifier, Blob);
        case TYPEINFO_EXCEPTION_SPEC:
            if(! I_->isFunction())
                return Error("wrong TypeInfo kind");
            return decodeRecord(R, static_cast<
                FunctionTypeInfo&>(*I_).ExceptionSpec, Blob);
        default:
            return AnyBlock::parseRecord(R, ID, Blob);
        }
    }

    Error
    readSubBlock(unsigned ID) override;
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I_.Type, br_);
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
    BitcodeReader& br_;
    std::unique_ptr<TArg>& I_;

public:
    TemplateArgBlock(
        std::unique_ptr<TArg>& I,
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
        case TEMPLATE_ARG_KIND:
        {
            TArgKind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            switch(kind)
            {
            case TArgKind::Type:
                I_ = std::make_unique<TypeTArg>();
                break;
            case TArgKind::NonType:
                I_ = std::make_unique<NonTypeTArg>();
                break;
            case TArgKind::Template:
                I_= std::make_unique<TemplateTArg>();
                break;
            default:
                return formatError("invalid template argument kind");
            }
            return Error::success();
        }
        case TEMPLATE_ARG_IS_PACK:
        {
            return decodeRecord(R, I_->IsPackExpansion, Blob);
        }
        case TEMPLATE_ARG_TEMPLATE:
        {
            if(! I_->isTemplate())
                return formatError("only TemplateTArgs may reference a template");
            return decodeRecord(R,
                static_cast<TemplateTArg&>(
                    *I_.get()).Template, Blob);
        }
        case TEMPLATE_ARG_NAME:
        {
            if(! I_->isTemplate())
                return formatError("only TemplateTArgs may have a template name");
            return decodeRecord(R,
                static_cast<TemplateTArg&>(
                    *I_.get()).Name, Blob);
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            if(! I_->isType())
                return formatError("only TypeTArgs may have types");
            TypeInfoBlock B(static_cast<TypeTArg&>(
                *I_.get()).Type, br_);
            return br_.readBlock(B, ID);
        }
        case BI_EXPR_BLOCK_ID:
        {
            if(! I_->isNonType())
                return formatError("only NonTypeTArgs may have expressions");
            ExprBlock B(static_cast<NonTypeTArg&>(
                *I_.get()).Value, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return AnyBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class TemplateParamBlock
    : public BitcodeReader::AnyBlock
{
    BitcodeReader& br_;
    std::unique_ptr<TParam>& I_;

public:
    TemplateParamBlock(
        std::unique_ptr<TParam>& I,
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
        case TEMPLATE_PARAM_KIND:
        {
            TParamKind kind{};
            if(auto err = decodeRecord(R, kind, Blob))
                return err;
            switch(kind)
            {
            case TParamKind::Type:
                I_ = std::make_unique<TypeTParam>();
                break;
            case TParamKind::NonType:
                I_ = std::make_unique<NonTypeTParam>();
                break;
            case TParamKind::Template:
                I_= std::make_unique<TemplateTParam>();
                break;
            default:
                return formatError("invalid template parameter kind");
            }
            return Error::success();
        }
        case TEMPLATE_PARAM_NAME:
        {
            return decodeRecord(R, I_->Name, Blob);
        }
        case TEMPLATE_PARAM_IS_PACK:
        {
            return decodeRecord(R, I_->IsParameterPack, Blob);
        }
        case TEMPLATE_PARAM_KEY_KIND:
        {
            if(! I_->isType())
                return formatError("only TypeTParams have a key kind");
            return decodeRecord(R, static_cast<
                TypeTParam&>(*I_.get()).KeyKind, Blob);
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
            if(! I_->isTemplate())
                return formatError("only TemplateTParam may have template parameters");
            TemplateParamBlock B(
                static_cast<TemplateTParam&>(
                    *I_.get()).Params.emplace_back(), br_);
            return br_.readBlock(B, ID);
        }
        case BI_TEMPLATE_ARG_BLOCK_ID:
        {
            TemplateArgBlock B(I_->Default, br_);
            return br_.readBlock(B, ID);
        }
        case BI_TYPEINFO_BLOCK_ID:
        {
            if(! I_->isNonType())
                return formatError("only NonTypeTParams may have a type");
            TypeInfoBlock B(static_cast<
                NonTypeTParam&>(*I_.get()).Type, br_);
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
            return decodeRecord(R, I_.Primary, Blob);
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
                I_.Args.emplace_back(), br_);
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

Error
TypeInfoBlock::
readSubBlock(unsigned ID)
{
    switch(ID)
    {
    // if the subblock ID is BI_TYPEINFO_BLOCK_ID,
    // it means that the block is a subblock of a
    // BI_TYPEINFO_CHILD_BLOCK_ID, BI_TYPEINFO_PARENT_BLOCK_ID,
    // or BI_TYPEINFO_PARAM_BLOCK_ID and should "forward"
    // the result to the caller
    case BI_TYPEINFO_BLOCK_ID:
        return br_.readBlock(*this, ID);
    case BI_TYPEINFO_CHILD_BLOCK_ID:
        return visit(*I_, [&]<typename T>(T& t)
            {
                std::unique_ptr<TypeInfo>* child = nullptr;
                if constexpr(requires { t.PointeeType; })
                    child = &t.PointeeType;
                else if constexpr(T::isArray())
                    child = &t.ElementType;
                else if constexpr(T::isFunction())
                    child = &t.ReturnType;

                if(! child)
                    return Error("wrong TypeInfo kind");
                TypeInfoBlock B(*child, br_);
                return br_.readBlock(B, ID);
            });
    case BI_TYPEINFO_PARENT_BLOCK_ID:
        return visit(*I_, [&]<typename T>(T& t)
            {
                if constexpr(requires { t.ParentType; })
                {
                    TypeInfoBlock B(t.ParentType, br_);
                    return br_.readBlock(B, ID);
                }
                return Error("wrong TypeInfo kind");
            });

    case BI_TYPEINFO_PARAM_BLOCK_ID:
    {
        if(! I_->isFunction())
            return Error("wrong TypeInfo kind");
        auto& I = static_cast<FunctionTypeInfo&>(*I_);
        TypeInfoBlock B(I.ParamTypes.emplace_back(), br_);
        return br_.readBlock(B, ID);
    }
    case BI_TEMPLATE_ARG_BLOCK_ID:
    {
        if(! I_->isSpecialization())
            return Error("wrong TypeInfo kind");
        auto& I = static_cast<SpecializationTypeInfo&>(*I_);
        TemplateArgBlock B(I.TemplateArgs.emplace_back(), br_);
        return br_.readBlock(B, ID);
    }
    case BI_EXPR_BLOCK_ID:
    {
        if(! I_->isArray())
            return Error("wrong TypeInfo kind");
        auto& I = static_cast<ArrayTypeInfo&>(*I_);
        ExprBlock B(I.Bounds, br_);
        return br_.readBlock(B, ID);
    }
    default:
        return AnyBlock::readSubBlock(ID);
    }
}

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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I_.Type, br_);
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
    std::unique_ptr<T> I = nullptr;

    explicit
    TopLevelBlock(
        BitcodeReader& br)
        : br_(br)
    {
    }

    Error readSubBlock(unsigned ID) override;
};

//------------------------------------------------

class NamespaceBlock
    : public TopLevelBlock<NamespaceInfo>
{
public:
    using TopLevelBlock::TopLevelBlock;

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
        case NAMESPACE_BITS:
            return decodeRecord(R, {&I->specs.raw}, Blob);
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
    using TopLevelBlock::TopLevelBlock;

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
    using TopLevelBlock::TopLevelBlock;

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FUNCTION_BITS:
            return decodeRecord(R, {&I->specs0.raw, &I->specs1.raw}, Blob);
        case FUNCTION_CLASS:
            return decodeRecord(R, I->Class, Blob);
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->ReturnType, br_);
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
    using TopLevelBlock::TopLevelBlock;

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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->Type, br_);
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

class EnumBlock
    : public TopLevelBlock<EnumInfo>
{
public:
    using TopLevelBlock::TopLevelBlock;

    Error
    parseRecord(Record const& R,
        unsigned ID, llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case ENUM_SCOPED:
            return decodeRecord(R, I->Scoped, Blob);
        case ENUM_MEMBERS:
            return decodeRecord(R, I->Members, Blob);
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->UnderlyingType, br_);
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
    using TopLevelBlock::TopLevelBlock;

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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->Type, br_);
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
    using TopLevelBlock::TopLevelBlock;

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FIELD_DEFAULT:
            return decodeRecord(R, I->Default, Blob);
        case FIELD_ATTRIBUTES:
            return decodeRecord(R, {&I->specs.raw}, Blob);
        case FIELD_IS_MUTABLE:
            return decodeRecord(R, I->IsMutable, Blob);
        case FIELD_IS_BITFIELD:
            return decodeRecord(R, I->IsBitfield, Blob);
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->Type, br_);
            return br_.readBlock(B, ID);
        }
        case BI_EXPR_BLOCK_ID:
        {
            ExprBlock B(I->BitfieldWidth, br_);
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
    using TopLevelBlock::TopLevelBlock;

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
            TemplateArgBlock B(I->Args.emplace_back(), br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class FriendBlock
    : public TopLevelBlock<FriendInfo>
{
public:
    using TopLevelBlock::TopLevelBlock;

    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        case FRIEND_SYMBOL:
            return decodeRecord(R, I->FriendSymbol, Blob);
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
        case BI_TYPEINFO_BLOCK_ID:
        {
            TypeInfoBlock B(I->FriendType, br_);
            return br_.readBlock(B, ID);
        }
        default:
            return TopLevelBlock::readSubBlock(ID);
        }
    }
};

//------------------------------------------------

class EnumeratorBlock
    : public TopLevelBlock<EnumeratorInfo>
{
public:
    using TopLevelBlock::TopLevelBlock;

    #if 0
    Error
    parseRecord(
        Record const& R,
        unsigned ID,
        llvm::StringRef Blob) override
    {
        switch(ID)
        {
        default:
            return TopLevelBlock::parseRecord(R, ID, Blob);
        }
    }
    #endif

    Error
    readSubBlock(
        unsigned ID) override
    {
        switch(ID)
        {
        case BI_EXPR_BLOCK_ID:
        {
            ExprBlock B(I->Initializer, br_);
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
            InfoPartBlock<T> B(I, br_);
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

} // mrdocs
} // clang

#endif
