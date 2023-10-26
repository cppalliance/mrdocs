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

#include "CXXTags.hpp"
#include "XMLWriter.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Support/Yaml.hpp"
#include "lib/Support/Radix.hpp"
#include "lib/Support/SafeNames.hpp"
#include <mrdocs/Platform.hpp>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

namespace clang {
namespace mrdocs {
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
} // mrdocs
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::xml::XMLWriter::XmlKey>
{
    static void mapping(IO& io,
        clang::mrdocs::xml::XMLWriter::XmlKey& opt_)
    {
        auto& opt= opt_.opt;
        io.mapOptional("index",  opt.index);
        io.mapOptional("prolog", opt.prolog);
        io.mapOptional("safe-names", opt.safe_names);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::xml::XMLWriter::GenKey>
{
    static void mapping(IO& io,
        clang::mrdocs::xml::XMLWriter::GenKey& opt)
    {
        clang::mrdocs::xml::XMLWriter::XmlKey k(opt.opt);

        io.mapOptional("xml",  k);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::xml::XMLWriter::Options>
{
    static void mapping(IO& io,
        clang::mrdocs::xml::XMLWriter::Options& opt)
    {
        clang::mrdocs::xml::XMLWriter::GenKey k(opt);
        io.mapOptional("generator",  k);
    }
};

namespace clang {
namespace mrdocs {
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
    YamlReporter reporter;

    {
        llvm::yaml::Input yin(
            corpus_.config->configYaml,
                &reporter, reporter);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return Error(ec);
    }
    {
        llvm::yaml::Input yin(
            corpus_.config->extraYaml,
                &reporter, reporter);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return Error(ec);
    }

    if(options_.prolog)
        os_ <<
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
            "<mrdocs xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdocs/raw/develop/mrdocs.rnc\">\n";

    if(options_.index || options_.safe_names)
        writeIndex();

    visit(corpus_.globalNamespace(), *this);

    if(options_.prolog)
        os_ << "</mrdocs>\n";

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
        SafeNames names(corpus_, true);
        for(auto& I : corpus_)
        {
            auto safe_name = names.getUnqualified(I.id);
            tags_.write("symbol", {}, {
                { "safe", safe_name },
                { "name", corpus_.getFullyQualifiedName(I, temp) },
                { "tag", toString(I.Kind) },
                { I.id } });
        }
    }
    else
    {
        for(auto& I : corpus_)
            tags_.write("symbol", {}, {
                { "name", corpus_.getFullyQualifiedName(I, temp) },
                { "tag", toString(I.Kind) },
                { I.id } });
    }
    tags_.close("symbols");
}

//------------------------------------------------

template<class T>
void
XMLWriter::
operator()(
    T const& I)
{
    if constexpr(T::isNamespace())
    {
        tags_.open(namespaceTagName, {
            { "name", I.Name, ! I.Name.empty() },
            { I.id },
            { "is-anonymous", "1", I.specs.isAnonymous},
            { "is-inline", "1", I.specs.isInline}
            });
        writeJavadoc(I.javadoc);
        corpus_.traverse(I, *this);
        tags_.close(namespaceTagName);
    }
    if constexpr(T::isRecord())
        writeRecord(I);
    if constexpr(T::isFunction())
        writeFunction(I);
    if constexpr(T::isEnum())
        writeEnum(I);
    if constexpr(T::isTypedef())
        writeTypedef(I);
    if constexpr(T::isField())
        writeField(I);
    if constexpr(T::isVariable())
        writeVar(I);
    if constexpr(T::isSpecialization())
        writeSpecialization(I);
}

//------------------------------------------------

void
XMLWriter::
writeEnum(
    EnumInfo const& I)
{
    tags_.open(enumTagName, {
        { "name", I.Name },
        { "class", "scoped", I.Scoped },
        { I.Access },
        { I.id }
    });
    if(I.UnderlyingType)
    {
        tags_.open(baseTagName);
        writeType(I.UnderlyingType, tags_);
        tags_.close(baseTagName);
    }

    writeSourceInfo(I);

    for(auto const& V : I.Members)
    {
        std::string val = V.Initializer.Value ?
            std::to_string(*V.Initializer.Value) :
            V.Initializer.Written;
        if(! V.javadoc)
        {
            tags_.write("value", {}, {
                { "name", V.Name },
                { "value", val },
                });
        }
        else
        {
            tags_.open("value", {
                { "name", V.Name },
                { "value", val }
            });
            writeJavadoc(V.javadoc);
            tags_.close("value");
        }
    }

    writeJavadoc(I.javadoc);

    tags_.close(enumTagName);
}

void
XMLWriter::
writeFunction(
    FunctionInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(functionTagName, {
        { "class", toString(I.Class),
            I.Class != FunctionClass::Normal },
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    write(I.specs0, tags_);
    write(I.specs1, tags_);

    writeReturnType(*I.ReturnType, tags_);

    for(auto const& J : I.Params)
        writeParam(J, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(functionTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeRecord(
    RecordInfo const& I)
{
    openTemplate(I.Template);

    auto tagName = toString(I.KeyKind);

    tags_.open(tagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    write(I.specs, tags_);

    for(auto const& B : I.Bases)
    {
        tags_.open(baseTagName, {
            { B.Access },
            { "class", "virtual", B.IsVirtual },
        });
        writeType(B.Type, tags_);
        tags_.close(baseTagName);
    }

    // Friends
    for(auto const& id : I.Friends)
        tags_.write(friendTagName, "", { { id } });

    writeJavadoc(I.javadoc);

    corpus_.traverse(I, *this);

    tags_.close(tagName);

    closeTemplate(I.Template);
}

void
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

    writeSourceInfo(I);

    writeType(I.Type, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(tag);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeField(
    const FieldInfo& I)
{
    std::string_view tag_name = dataMemberTagName;
    std::string bit_width;
    if(I.IsBitfield)
    {
        tag_name = bitfieldTagName;
        bit_width = I.BitfieldWidth.Value ?
            std::to_string(*I.BitfieldWidth.Value) :
            I.BitfieldWidth.Written;
    }

    tags_.open(tag_name, {
        { "name", I.Name },
        { I.Access },
        { I.id },
        { "width", bit_width, I.IsBitfield },
        { "default", I.Default, ! I.Default.empty() }
    });

    writeSourceInfo(I);

    if(I.IsMutable)
        tags_.write(attributeTagName, {}, {
            {"id", "is-mutable"}
        });

    write(I.specs, tags_);

    writeType(I.Type, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(tag_name);
}

void
XMLWriter::
writeVar(
    VariableInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(varTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    write(I.specs, tags_);

    writeType(I.Type, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(varTagName);

    closeTemplate(I.Template);
}

//------------------------------------------------

void
XMLWriter::
writeSourceInfo(
    SourceInfo const& I)
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

    const SymbolID& id = I->Primary ?
        *I->Primary : SymbolID::zero;

    tags_.open(templateTagName, {
        {"class", toString(I->specializationKind()),
            I->specializationKind() != TemplateSpecKind::Primary},
        {id}
    });

    for(const auto& tparam : I->Params)
        writeTemplateParam(*tparam, tags_);
    for(const auto& targ : I->Args)
        writeTemplateArg(*targ, tags_);
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

void
XMLWriter::
writeSpecialization(
    const SpecializationInfo& I)
{
    tags_.open(specializationTagName, {
        {I.id},
        {"primary", toString(I.Primary) }
    });

    for(const auto& targ : I.Args)
        writeTemplateArg(*targ, tags_);

    corpus_.traverse(I, *this);

    tags_.close(specializationTagName);
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
    writeNodes(javadoc->getBlocks());
    tags_.close(javadocTagName);
}

template<class T>
void
XMLWriter::
writeNodes(
    doc::List<T> const& list)
{
    if(list.empty())
        return;
    for(auto const& node : list)
        writeNode(*node);
}

void
XMLWriter::
writeNode(
    doc::Node const& node)
{
    switch(node.kind)
    {
    case doc::Kind::text:
        writeText(static_cast<doc::Text const&>(node));
        break;
    case doc::Kind::styled:
        writeStyledText(static_cast<doc::Styled const&>(node));
        break;
    case doc::Kind::heading:
        writeHeading(static_cast<doc::Heading const&>(node));
        break;
    case doc::Kind::paragraph:
        writeParagraph(static_cast<doc::Paragraph const&>(node));
        break;
    case doc::Kind::link:
        writeLink(static_cast<doc::Link const&>(node));
        break;
    case doc::Kind::list_item:
        writeListItem(static_cast<doc::ListItem const&>(node));
        break;
    case doc::Kind::brief:
        writeBrief(static_cast<doc::Brief const&>(node));
        break;
    case doc::Kind::admonition:
        writeAdmonition(static_cast<doc::Admonition const&>(node));
        break;
    case doc::Kind::code:
        writeCode(static_cast<doc::Code const&>(node));
        break;
    case doc::Kind::param:
        writeJParam(static_cast<doc::Param const&>(node));
        break;
    case doc::Kind::tparam:
        writeTParam(static_cast<doc::TParam const&>(node));
        break;
    case doc::Kind::returns:
        writeReturns(static_cast<doc::Returns const&>(node));
        break;
    default:
        // unknown kind
        MRDOCS_UNREACHABLE();
    }
}

void
XMLWriter::
writeLink(
    doc::Link const& node)
{
    tags_.write("link", node.string, {
        { "href", node.href }
        });
}

void
XMLWriter::
writeListItem(
    doc::ListItem const& node)
{
    tags_.open("listitem");
    writeNodes(node.children);
    tags_.close("listitem");
}

void
XMLWriter::
writeBrief(
    doc::Paragraph const& node)
{
    tags_.open("brief");
    writeNodes(node.children);
    tags_.close("brief");
}

void
XMLWriter::
writeText(
    doc::Text const& node)
{
    tags_.indent() <<
        "<text>" <<
        xmlEscape(node.string) <<
        "</text>\n";
}

void
XMLWriter::
writeStyledText(
    doc::Styled const& node)
{
    tags_.write(toString(node.style), node.string);
}

void
XMLWriter::
writeHeading(
    doc::Heading const& heading)
{
    tags_.write("head", heading.string);
}

void
XMLWriter::
writeParagraph(
    doc::Paragraph const& para,
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
    doc::Admonition const& admonition)
{
    llvm::StringRef tag;
    switch(admonition.admonish)
    {
    case doc::Admonish::note:
        tag = "note";
        break;
    case doc::Admonish::tip:
        tag = "tip";
        break;
    case doc::Admonish::important:
        tag = "important";
        break;
    case doc::Admonish::caution:
        tag = "caution";
        break;
    case doc::Admonish::warning:
        tag = "warning";
        break;
    default:
        // unknown style
        MRDOCS_UNREACHABLE();
    }
    writeParagraph(admonition, tag);
}

void
XMLWriter::
writeCode(doc::Code const& code)
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
    doc::Returns const& returns)
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
    doc::Param const& param)
{
    dom::String direction;
    switch(param.direction)
    {
    case doc::ParamDirection::none:
        direction = "";
        break;
    case doc::ParamDirection::in:
        direction = "in";
        break;
    case doc::ParamDirection::out:
        direction = "out";
        break;
    case doc::ParamDirection::inout:
        direction = "inout";
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    tags_.open("param", {
        { "name", param.name, ! param.name.empty() },
        { "class", direction, ! direction.empty() }
    });
    writeNodes(param.children);
    tags_.close("param");
}

void
XMLWriter::
writeTParam(
    doc::TParam const& tparam)
{
    tags_.open("tparam", {
        { "name", tparam.name, ! tparam.name.empty() }});
    writeNodes(tparam.children);
    tags_.close("tparam");
}

} // xml
} // mrdocs
} // clang
