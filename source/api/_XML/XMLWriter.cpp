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

#include "XMLWriter.hpp"
#include "ConfigImpl.hpp"
#include "CXXTags.hpp"
#include "Support/Radix.hpp"
#include "Support/Operator.hpp"
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

namespace clang {
namespace mrdox {
namespace xml {

struct XMLWriter::XmlKey
{
    Options& opt;

    explicit
    XmlKey(
        Options& opt_) noexcept
        : opt(opt_)
    {
    }
};

struct XMLWriter::GenKey
{
    Options& opt;

    explicit
    GenKey(
        Options& opt_)
        : opt(opt_)
    {
    }
};

} // xml
} // mrdox
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::xml::XMLWriter::XmlKey>
{
    static void mapping(IO& io,
        clang::mrdox::xml::XMLWriter::XmlKey& opt_)
    {
        auto& opt= opt_.opt;
        io.mapOptional("index",  opt.index);
        io.mapOptional("prolog", opt.prolog);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::xml::XMLWriter::GenKey>
{
    static void mapping(IO& io,
        clang::mrdox::xml::XMLWriter::GenKey& opt)
    {
        clang::mrdox::xml::XMLWriter::XmlKey k(opt.opt);

        io.mapOptional("xml",  k);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::xml::XMLWriter::Options>
{
    static void mapping(IO& io,
        clang::mrdox::xml::XMLWriter::Options& opt)
    {
        clang::mrdox::xml::XMLWriter::GenKey k(opt);
        io.mapOptional("generator",  k);
    }
};

namespace clang {
namespace mrdox {
namespace xml {

//------------------------------------------------
//
// XMLWriter
//
//------------------------------------------------

XMLWriter::
XMLWriter(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : tags_(os)
    , os_(os)
    , corpus_(corpus)
    , R_(R)
{
}

Err
XMLWriter::
build()
{
    {
        llvm::yaml::Input yin(
            corpus_.config.configYaml,
                this, ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return makeErr(ec);
    }
    {
        llvm::yaml::Input yin(
            corpus_.config.extraYaml,
                this, ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return makeErr(ec);
    }

    if(options_.prolog)
        os_ <<
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
            "<mrdox xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdox/raw/develop/mrdox.rnc\">\n";

    if(options_.index)
        writeIndex();

    if(! corpus_.visit(globalNamespaceID, *this))
        return makeErr("visit failed");

    if(options_.prolog)
        os_ << "</mrdox>\n";

    return {};
}

//------------------------------------------------

void
XMLWriter::
writeIndex()
{
    std::string temp;
    temp.reserve(256);
    tags_.open("symbols");
    for(auto I : corpus_.index())
        tags_.write("symbol", {}, {
            { "name", I->getFullyQualifiedName(temp) },
            { "tag", getTagName(*I) },
            { I->id } });
    tags_.close("symbols");
}

//------------------------------------------------

bool
XMLWriter::
visit(
    NamespaceInfo const& I)
{
    tags_.open(namespaceTagName, {
        { "name", I.Name },
        { I.id }
        });

    writeInfo(I);

    writeJavadoc(I.javadoc);

    if(! corpus_.visit(I.Children, *this))
        return false;

    tags_.close(namespaceTagName);

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    RecordInfo const& I)
{
    llvm::StringRef tagName;
    switch(I.TagType)
    {
    case TagTypeKind::TTK_Class:  tagName = classTagName; break;
    case TagTypeKind::TTK_Struct: tagName = structTagName; break;
    case TagTypeKind::TTK_Union:  tagName = unionTagName; break;
    default:
        Assert(false);
    }
    tags_.open(tagName, {
        { "name", I.Name },
        { I.id }
        });

    writeSymbol(I);

    write(I.specs, tags_);

    if(I.Template)
        for(TemplateParamInfo const& J : I.Template->Params)
            writeTemplateParam(J);

    for(auto const& J : I.Bases)
        writeBaseRecord(J);
    // VFALCO data members?
    for(auto const& J : I.Members)
        writeMemberType(J);

    // Friends
    for(auto const& id : I.Friends)
        tags_.write(friendTagName, "", { { id } });

    writeJavadoc(I.javadoc);

    if(! corpus_.visit(I.Children, *this))
        return false;

    tags_.close(tagName);

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    FunctionInfo const& I)
{
    tags_.open(functionTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);

    write(I.specs0, tags_);

    writeReturnType(I.ReturnType, tags_);

    for(auto const& J : I.Params)
        writeParam(J, tags_);

    if(I.Template)
        for(TemplateParamInfo const& J : I.Template->Params)
            writeTemplateParam(J);

    writeJavadoc(I.javadoc);

    tags_.close(functionTagName);

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    TypedefInfo const& I)
{
    llvm::StringRef tag;
    if(I.IsUsing)
        tag = aliasTagName;
    else
        tag = typedefTagName;
    tags_.open(tag, {
        { "name", I.Name },
        { I.id }
        });

    writeSymbol(I);

    tags_.write("type", "", {
        { "name", I.Underlying.Type.Name },
        { I.Underlying.Type.id } });

    writeJavadoc(I.javadoc);

    tags_.close(tag);

    return true;
}

bool
XMLWriter::
visit(
    EnumInfo const& I)
{
    tags_.open(enumTagName, {
        { "name", I.Name },
        { "class", "scoped", I.Scoped },
        { I.BaseType },
        { I.id }
        });

    writeSymbol(I);

    for(auto const& v : I.Members)
        tags_.write("element", {}, {
            { "name", v.Name },
            { "value", v.Value },
            });

    writeJavadoc(I.javadoc);

    tags_.close(enumTagName);

    return true;
}

bool
XMLWriter::
visit(
    VariableInfo const& I)
{
    tags_.open(variableTagName, {
        { "name", I.Name },
        { I.id }
        });

    writeSymbol(I);

    write(I.specs, tags_);

    tags_.write("type", {}, {
        { "name", I.Type.Name },
        { I.Type.id }
        });

    writeJavadoc(I.javadoc);

    tags_.close(variableTagName);

    return true;
}

//------------------------------------------------

void
XMLWriter::
writeInfo(
    Info const& I)
{
}

void
XMLWriter::
writeSymbol(
    SymbolInfo const& I)
{
    writeInfo(I);
    if(I.DefLoc)
        writeLocation(*I.DefLoc, true);
    for(auto const& loc : I.Loc)
        writeLocation(loc, false);
}

void
XMLWriter::
writeLocation(
    Location const& loc,
    bool def)
{
    tags_.write("file", {}, {
        { "path", loc.Filename },
        { "line", std::to_string(loc.LineNumber) },
        { "class", "def", def } });
}

void
XMLWriter::
writeBaseRecord(
    BaseRecordInfo const& I)
{
    tags_.write("base", {}, {
        { "name", I.Name },
        { I.Access },
        { "modifier", "virtual", I.IsVirtual },
        { I.id } });
    if(! corpus_.exists(I.id))
    {
        // e,g. for std::true_type
        return;
    }
}

void
XMLWriter::
writeTemplateParam(
    TemplateParamInfo const& I)
{
    tags_.write(tparamTagName, {}, {
        { "decl", I.Contents }
        });
}

void
XMLWriter::
writeMemberType(
    MemberTypeInfo const& I)
{
    tags_.write(dataMemberTagName, {}, {
        { "name", I.Name },
        { "type", I.Type.Name },
        { "value", I.DefaultValue, ! I.DefaultValue.empty() },
        { I.Access },
        { I.Type.id } });
}

//------------------------------------------------

void
XMLWriter::
writeJavadoc(
    llvm::Optional<Javadoc> const& javadoc)
{
    if(! javadoc.has_value())
        return;
    tags_.open(javadocTagName);
    if(auto brief = javadoc->getBrief())
        writeBrief(*brief);
    writeNodes(javadoc->getBlocks());
    if(auto returns = javadoc->getReturns())
        writeNode(*returns);
    writeNodes(javadoc->getParams());
    writeNodes(javadoc->getTParams());
    tags_.close(javadocTagName);
}

template<class T>
void
XMLWriter::
writeNodes(
    AnyList<T> const& list)
{
    if(list.empty())
        return;
    for(auto const& node : list)
        writeNode(node);
}

void
XMLWriter::
writeNode(
    Javadoc::Node const& node)
{
    switch(node.kind)
    {
    case Javadoc::Kind::text:
        writeText(static_cast<Javadoc::Text const&>(node));
        break;
    case Javadoc::Kind::styled:
        writeStyledText(static_cast<Javadoc::StyledText const&>(node));
        break;
    case Javadoc::Kind::paragraph:
        writeParagraph(static_cast<Javadoc::Paragraph const&>(node));
        break;
    case Javadoc::Kind::brief:
        writeBrief(static_cast<Javadoc::Brief const&>(node));
        break;
    case Javadoc::Kind::admonition:
        writeAdmonition(static_cast<Javadoc::Admonition const&>(node));
        break;
    case Javadoc::Kind::code:
        writeCode(static_cast<Javadoc::Code const&>(node));
        break;
    case Javadoc::Kind::param:
        writeJParam(static_cast<Javadoc::Param const&>(node));
        break;
    case Javadoc::Kind::tparam:
        writeTParam(static_cast<Javadoc::TParam const&>(node));
        break;
    case Javadoc::Kind::returns:
        writeReturns(static_cast<Javadoc::Returns const&>(node));
        break;
    default:
        llvm_unreachable("unknown kind");
    }
}

void
XMLWriter::
writeBrief(
    Javadoc::Paragraph const& node)
{
    tags_.open("brief");
    writeNodes(node.children);
    tags_.close("brief");
}

void
XMLWriter::
writeText(
    Javadoc::Text const& node)
{
    tags_.indent() <<
        "<text>" <<
        xmlEscape(node.string) <<
        "</text>\n";
}

void
XMLWriter::
writeStyledText(
    Javadoc::StyledText const& node)
{
    tags_.write(toString(node.style), node.string);
}

void
XMLWriter::
writeParagraph(
    Javadoc::Paragraph const& para,
    llvm::StringRef tag)
{
    tags_.open("para", {
        { "class", tag, ! tag.empty() }});
    writeNodes(para.children);
    tags_.close("para");
}

void
XMLWriter::
writeAdmonition(
    Javadoc::Admonition const& admonition)
{
    llvm::StringRef tag;
    switch(admonition.style)
    {
    case Javadoc::Admonish::note:
        tag = "note";
        break;
    case Javadoc::Admonish::tip:
        tag = "tip";
        break;
    case Javadoc::Admonish::important:
        tag = "important";
        break;
    case Javadoc::Admonish::caution:
        tag = "caution";
        break;
    case Javadoc::Admonish::warning:
        tag = "warning";
        break;
    default:
        llvm_unreachable("unknown style");
    }
    writeParagraph(admonition, tag);
}

void
XMLWriter::
writeCode(Javadoc::Code const& code)
{
    if(code.children.empty())
    {
        tags_.indent() << "<code/>\n";
        return;
    }

    tags_.open("code");
    writeNodes(code.children);
    tags_.close("code");
}

void
XMLWriter::
writeReturns(
    Javadoc::Returns const& returns)
{
    if(returns.empty())
        return;
    tags_.open("returns");
    writeNodes(returns.children);
    tags_.close("returns");
}

void
XMLWriter::
writeJParam(
    Javadoc::Param const& param)
{
    tags_.open("param", {
        { "name", param.name, ! param.name.empty() }});
    writeNodes(param.children);
    tags_.close("param");
}

void
XMLWriter::
writeTParam(
    Javadoc::TParam const& tparam)
{
    tags_.open("tparam", {
        { "name", tparam.name, ! tparam.name.empty() }});
    writeNodes(tparam.children);
    tags_.close("tparam");
}

} // xml
} // mrdox
} // clang
