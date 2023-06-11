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
#include "Tool/ConfigImpl.hpp"
#include "CXXTags.hpp"
#include "Support/Radix.hpp"
#include "Support/SafeNames.hpp"
#include "Support/Operator.hpp"
#include <mrdox/Platform.hpp>
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
        io.mapOptional("safe-names", opt.safe_names);
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
    Corpus const& corpus) noexcept
    : tags_(os)
    , os_(os)
    , corpus_(corpus)
{
}

Error
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
            return Error(ec);
    }
    {
        llvm::yaml::Input yin(
            corpus_.config.extraYaml,
                this, ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return Error(ec);
    }

    if(options_.prolog)
        os_ <<
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
            "<mrdox xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdox/raw/develop/mrdox.rnc\">\n";

    if(options_.index || options_.safe_names)
        writeIndex();

    if(! corpus_.traverse(*this, SymbolID::zero))
        return Error("visitation aborted");

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
    if(options_.safe_names)
    {
        SafeNames names(corpus_);
        for(auto I : corpus_.index())
        {
            auto safe_name = names.get(I->id);
            tags_.write("symbol", {}, {
                { "safe", safe_name },
                { "name", corpus_.getFullyQualifiedName(*I, temp) },
                { "tag", getTagName(*I) },
                { I->id } });
        }
    }
    else
    {
        for(auto I : corpus_.index())
            tags_.write("symbol", {}, {
                { "name", corpus_.getFullyQualifiedName(*I, temp) },
                { "tag", getTagName(*I) },
                { I->id } });
    }
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

    writeJavadoc(I.javadoc);

    if(! corpus_.traverse(*this, I))
        return false;

    tags_.close(namespaceTagName);

    return true;
}

bool
XMLWriter::
visit(
    RecordInfo const& I)
{
    return writeRecord(I);
}

bool
XMLWriter::
visit(
    FunctionInfo const& I)
{
    return writeFunction(I);
}

bool
XMLWriter::
visit(
    TypedefInfo const& I)
{
    return writeTypedef(I);
}

bool
XMLWriter::
visit(
    EnumInfo const& I)
{
    return writeEnum(I);
}

bool
XMLWriter::
visit(
    FieldInfo const& I)
{
    return writeField(I);
}

bool
XMLWriter::
visit(
    VarInfo const& I)
{
    return writeVar(I);
}

bool
XMLWriter::
visit(
    SpecializationInfo const& I)
{
    return writeSpecialization(I);
}

//------------------------------------------------

bool
XMLWriter::
writeEnum(
    EnumInfo const& I)
{
    tags_.open(enumTagName, {
        { "name", I.Name },
        { "class", "scoped", I.Scoped },
        { I.BaseType },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);

    for(auto const& v : I.Members)
        tags_.write("value", {}, {
            { "name", v.Name },
            { "value", v.Value },
            });

    writeJavadoc(I.javadoc);

    tags_.close(enumTagName);
    return true;
}

bool
XMLWriter::
writeFunction(
    FunctionInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(functionTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);

    write(I.specs0, tags_);
    write(I.specs1, tags_);

    writeReturnType(I.ReturnType, tags_);

    for(auto const& J : I.Params)
        writeParam(J, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(functionTagName);

    closeTemplate(I.Template);
    return true;
}

bool
XMLWriter::
writeRecord(
    RecordInfo const& I)
{
    openTemplate(I.Template);

    llvm::StringRef tagName;
    switch(I.KeyKind)
    {
    case RecordKeyKind::Class:     tagName = classTagName; break;
    case RecordKeyKind::Struct:    tagName = structTagName; break;
    case RecordKeyKind::Union:     tagName = unionTagName; break;
    default:
        MRDOX_ASSERT(false);
    }
    tags_.open(tagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);


    write(I.specs, tags_);

    for(auto const& B : I.Bases)
        tags_.write(baseTagName, "", {
            { "name", B.Name },
            { B.Access },
            { "class", "virtual", B.IsVirtual },
            { B.id }
            });

    // Friends
    for(auto const& id : I.Friends)
        tags_.write(friendTagName, "", { { id } });

    writeJavadoc(I.javadoc);

    if(! corpus_.traverse(*this, I))
        return false;

    tags_.close(tagName);

    closeTemplate(I.Template);

    return true;
}

bool
XMLWriter::
writeTypedef(
    TypedefInfo const& I)
{
    openTemplate(I.Template);

    llvm::StringRef tag;
    if(I.IsUsing)
        tag = aliasTagName;
    else
        tag = typedefTagName;
    tags_.open(tag, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);

    tags_.write("type", "", {
        { "name", I.Underlying.Name },
        { I.Underlying.id } });

    writeJavadoc(I.javadoc);

    tags_.close(tag);

    closeTemplate(I.Template);

    return true;
}

bool
XMLWriter::
writeField(
    const FieldInfo& I)
{
    tags_.open(dataMemberTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id },
        { "default", I.Default, ! I.Default.empty() }
    });

    writeSymbol(I);

    write(I.specs, tags_);

    tags_.write("type", {}, {
        { "name", I.Type.Name },
        { I.Type.id }
        });

    writeJavadoc(I.javadoc);

    tags_.close(dataMemberTagName);

    return true;
}

bool
XMLWriter::
writeVar(
    VarInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(varTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSymbol(I);

    write(I.specs, tags_);

    tags_.write("type", {}, {
        { "name", I.Type.Name },
        { I.Type.id }
        });

    writeJavadoc(I.javadoc);

    tags_.close(varTagName);

    closeTemplate(I.Template);

    return true;
}

//------------------------------------------------

void
XMLWriter::
writeSymbol(
    SymbolInfo const& I)
{
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

//------------------------------------------------

void
XMLWriter::
openTemplate(
    const std::unique_ptr<TemplateInfo>& I)
{
    if(! I)
        return;
    const char* spec = nullptr;
    switch(I->specializationKind())
    {
    case TemplateSpecKind::Explicit:
        spec = "explicit";
        break;
    case TemplateSpecKind::Partial:
        spec = "partial";
        break;
    default:
        break;
    }
    const SymbolID& id = I->Primary ?
        *I->Primary : SymbolID::zero;

    tags_.open(templateTagName, {
        {"class", spec, !! spec},
        {id}
    });

    for(const TParam& tparam : I->Params)
        writeTemplateParam(tparam, tags_);
    for(const TArg& targ : I->Args)
        writeTemplateArg(targ, tags_);
}

void
XMLWriter::
closeTemplate(
    const std::unique_ptr<TemplateInfo>& I)
{
    if(! I)
        return;
    tags_.close(templateTagName);
}

bool
XMLWriter::
writeSpecialization(
    const SpecializationInfo& I)
{
    tags_.open(specializationTagName, {
        {I.id},
        {"primary", toString(I.Primary) }
    });

    for(const TArg& targ : I.Args)
        writeTemplateArg(targ, tags_);

    if(! corpus_.traverse(*this, I))
        return false;

    tags_.close(specializationTagName);

    return true;
}

//------------------------------------------------

void
XMLWriter::
writeJavadoc(
    std::unique_ptr<Javadoc> const& javadoc)
{
    if(! javadoc)
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
        // unknown kind
        MRDOX_UNREACHABLE();
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
        // unknown style
        MRDOX_UNREACHABLE();
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
    const char* direction = nullptr;
    switch(param.direction)
    {
    case Javadoc::ParamDirection::in:
        direction = "in";
        break;
    case Javadoc::ParamDirection::out:
        direction = "out";
        break;
    case Javadoc::ParamDirection::inout:
        direction = "inout";
        break;
    default:
        break;
    }
    tags_.open("param", {
        { "name", param.name, ! param.name.empty() },
        { "class", direction, !! direction }
    });
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
