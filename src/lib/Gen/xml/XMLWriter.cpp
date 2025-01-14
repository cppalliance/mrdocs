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
#include "lib/Support/LegibleNames.hpp"
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
        io.mapOptional("legible-names", opt.legible_names);
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

Expected<void>
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
        auto ec = yin.error();
        MRDOCS_CHECK(!ec, ec);
    }

    if(options_.prolog)
        os_ <<
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
            "<mrdocs xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdocs/raw/develop/mrdocs.rnc\">\n";

    if(options_.index || options_.legible_names)
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
    if(options_.legible_names)
    {
        LegibleNames const names(corpus_, true);
        for(auto& I : corpus_)
        {
            corpus_.qualifiedName(I, temp);
            auto legible_name = names.getUnqualified(I.id);
            tags_.write("symbol", {}, {
                { "legible", legible_name },
                { "name", temp },
                { "tag", toString(I.Kind) },
                { I.id } });
        }
    }
    else
    {
        for(auto& I : corpus_)
        {
            corpus_.qualifiedName(I, temp);
            tags_.write("symbol", {}, {
                { "name", temp },
                { "tag", toString(I.Kind) },
                { I.id } });
        }
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
    Info const& base = I;
    if (base.Extraction == ExtractionMode::Dependency)
    {
        return;
    }


    #define INFO(Type) if constexpr(T::is##Type()) write##Type(I);
    #include <mrdocs/Metadata/InfoNodesPascal.inc>
}

//------------------------------------------------

void
XMLWriter::
writeNamespace(
    NamespaceInfo const& I)
{
    tags_.open(namespaceTagName, {
        { "name", I.Name, ! I.Name.empty() },
        { I.id },
        { "is-anonymous", "1", I.IsAnonymous},
        { "is-inline", "1", I.IsInline}
    });
    writeJavadoc(I.javadoc);
    for(const SymbolID& id : I.UsingDirectives)
        tags_.write("using-directive", {}, { { id } });
    corpus_.traverse(I, *this);
    tags_.close(namespaceTagName);
}

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

    writeJavadoc(I.javadoc);

    corpus_.traverse(I, *this);

    tags_.close(enumTagName);
}

void
XMLWriter::
writeEnumConstant(
    EnumConstantInfo const& I)
{
    std::string val = I.Initializer.Value ?
        std::to_string(*I.Initializer.Value) :
        I.Initializer.Written;

    tags_.open(enumConstantTagName, {
        { "name", I.Name },
        { "initializer", val },
        { I.Access },
        { I.id }
    });

    writeSourceInfo(I);

    writeJavadoc(I.javadoc);

    tags_.close(enumConstantTagName);
}

void
XMLWriter::
writeFriend(
    FriendInfo const& I)
{
    tags_.open(friendTagName, {
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    writeJavadoc(I.javadoc);

    Attributes attrs = {};
    if(I.FriendSymbol)
        attrs.push({I.FriendSymbol});
    else if(I.FriendType)
        attrs.push({"type", toString(*I.FriendType)});

    tags_.write("befriended", {}, attrs);

    tags_.close(friendTagName);
}

void
XMLWriter::
writeFunction(
    FunctionInfo const& I)
{
    openTemplate(I.Template);

    auto except_spec = toString(I.Noexcept);
    auto explicit_spec = toString(I.Explicit);

    tags_.open(functionTagName, {
        { "class", toString(I.Class),
            I.Class != FunctionClass::Normal },
        { "name", I.Name },
        { I.Access },
        { "exception-spec", except_spec,
            ! except_spec.empty() },
        { "explicit-spec", explicit_spec,
            ! explicit_spec.empty() },
        { "requires", I.Requires.Written,
            ! I.Requires.Written.empty() },
        { I.id }
        });

    writeSourceInfo(I);

    writeAttr(I.IsVariadic,            "is-variadic", tags_);
    writeAttr(I.IsVirtualAsWritten,    "is-virtual-as-written", tags_);
    writeAttr(I.IsPure,                "is-pure", tags_);
    writeAttr(I.IsDefaulted,           "is-defaulted", tags_);
    writeAttr(I.IsExplicitlyDefaulted, "is-explicitly-defaulted", tags_);
    writeAttr(I.IsDeleted,             "is-deleted", tags_);
    writeAttr(I.IsDeletedAsWritten,    "is-deleted-as-written", tags_);
    writeAttr(I.IsNoReturn,            "is-no-return", tags_);
    writeAttr(I.HasOverrideAttr,       "has-override", tags_);
    writeAttr(I.HasTrailingReturn,     "has-trailing-return", tags_);
    writeAttr(I.Constexpr,             "constexpr-kind", tags_);
    writeAttr(I.OverloadedOperator,    "operator", tags_);
    writeAttr(I.StorageClass,          "storage-class", tags_);
    writeAttr(I.IsConst,               "is-const", tags_);
    writeAttr(I.IsVolatile,            "is-volatile", tags_);
    writeAttr(I.RefQualifier,          "ref-qualifier", tags_);
    writeAttr(I.IsNodiscard,           "nodiscard", tags_);
    writeAttr(I.IsExplicitObjectMemberFunction, "is-explicit-object-member-function", tags_);

    writeReturnType(*I.ReturnType, tags_);

    for(auto const& J : I.Params)
        writeParam(J, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(functionTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeGuide(
    GuideInfo const& I)
{
    openTemplate(I.Template);

    auto explicit_spec = toString(I.Explicit);

    tags_.open(guideTagName, {
        { "name", I.Name },
        { I.Access },
        { "explicit-spec", explicit_spec,
            ! explicit_spec.empty() },
        { I.id }
        });

    writeSourceInfo(I);

    tags_.open(deducedTagName);
    writeType(I.Deduced, tags_);
    tags_.close(deducedTagName);

    for(auto const& J : I.Params)
        writeParam(J, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(guideTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeConcept(
    ConceptInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(conceptTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id },
        { "constraint", I.Constraint.Written },
        });

    writeSourceInfo(I);

    writeJavadoc(I.javadoc);

    tags_.close(conceptTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeNamespaceAlias(
    NamespaceAliasInfo const& I)
{
    tags_.open(namespaceAliasTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    writeJavadoc(I.javadoc);

    tags_.write("aliased", {}, {
        {"name", toString(*I.AliasedSymbol)},
        { I.AliasedSymbol->id }
    });
    tags_.close(namespaceAliasTagName);
}

void
XMLWriter::
    writeUsing(UsingInfo const& I)
{
    dom::String classStr;
    switch (I.Class)
    {
    case UsingClass::Normal:
        classStr = "using";
        break;
    case UsingClass::Typename:
        classStr = "using typename";
        break;
    case UsingClass::Enum:
        classStr = "using enum";
        break;
    default:
        MRDOCS_UNREACHABLE();
    }

    std::string qualifierStr;
    if (I.Qualifier)
    {
        qualifierStr = toString(*I.Qualifier);
    }

    tags_.open(usingTagName, {
        { I.Access },
        { I.id },
        { "class", classStr, ! classStr.empty() },
        { "qualifier", qualifierStr, !qualifierStr.empty() }
    });

    writeSourceInfo(I);

    writeJavadoc(I.javadoc);

    for (auto const& id : I.UsingSymbols)
        tags_.write("named", {}, { id });

    tags_.close(usingTagName);
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

    writeAttr(I.IsFinal, "is-final", tags_);
    writeAttr(I.IsFinalDestructor, "is-final-dtor", tags_);

    for(auto const& B : I.Bases)
    {
        tags_.open(baseTagName, {
            { B.Access },
            { "class", "virtual", B.IsVirtual },
        });
        writeType(B.Type, tags_);
        tags_.close(baseTagName);
    }

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
        tag = namespaceAliasTagName;
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
        { "default", I.Default.Written, ! I.Default.Written.empty() }
    });

    writeSourceInfo(I);

    if(I.IsMutable)
        tags_.write(attributeTagName, {}, {
            {"id", "is-mutable"}
        });

    writeAttr(I.IsVariant, "is-variant", tags_);
    writeAttr(I.IsMaybeUnused, "maybe-unused", tags_);
    writeAttr(I.IsDeprecated, "deprecated", tags_);
    writeAttr(I.HasNoUniqueAddress, "no-unique-address", tags_);

    writeType(I.Type, tags_);

    writeJavadoc(I.javadoc);

    tags_.close(tag_name);
}

void
XMLWriter::
writeVariable(
    VariableInfo const& I)
{
    openTemplate(I.Template);

    tags_.open(varTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I);

    writeAttr(I.StorageClass, "storage-class", tags_);
    writeAttr(I.IsInline, "is-inline", tags_);
    writeAttr(I.IsConstexpr, "is-constexpr", tags_);
    writeAttr(I.IsConstinit, "is-constinit", tags_);
    writeAttr(I.IsThreadLocal, "is-thread-local", tags_);

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
    const std::optional<TemplateInfo>& I)
{
    if(! I)
        return;

    tags_.open(templateTagName, {
        {"class", toString(I->specializationKind()),
            I->specializationKind() != TemplateSpecKind::Primary},
        {"requires", I->Requires.Written,
            ! I->Requires.Written.empty()},
        {I->Primary}
    });

    for(const auto& tparam : I->Params)
        writeTemplateParam(*tparam, tags_);
    for(const auto& targ : I->Args)
        writeTemplateArg(*targ, tags_);
}

void
XMLWriter::
closeTemplate(
    const std::optional<TemplateInfo>& I)
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
        writeText(dynamic_cast<doc::Text const&>(node));
        break;
    case doc::Kind::styled:
        writeStyledText(dynamic_cast<doc::Styled const&>(node));
        break;
    case doc::Kind::heading:
        writeHeading(dynamic_cast<doc::Heading const&>(node));
        break;
    case doc::Kind::paragraph:
        writeParagraph(dynamic_cast<doc::Paragraph const&>(node));
        break;
    case doc::Kind::link:
        writeLink(dynamic_cast<doc::Link const&>(node));
        break;
    case doc::Kind::list_item:
        writeListItem(dynamic_cast<doc::ListItem const&>(node));
        break;
    case doc::Kind::brief:
        writeBrief(dynamic_cast<doc::Brief const&>(node));
        break;
    case doc::Kind::admonition:
        writeAdmonition(dynamic_cast<doc::Admonition const&>(node));
        break;
    case doc::Kind::code:
        writeCode(dynamic_cast<doc::Code const&>(node));
        break;
    case doc::Kind::param:
        writeJParam(dynamic_cast<doc::Param const&>(node));
        break;
    case doc::Kind::tparam:
        writeTParam(dynamic_cast<doc::TParam const&>(node));
        break;
    case doc::Kind::returns:
        writeReturns(dynamic_cast<doc::Returns const&>(node));
        break;
    case doc::Kind::reference:
        writeReference(dynamic_cast<doc::Reference const&>(node));
        break;
    case doc::Kind::copied:
        writeCopied(dynamic_cast<doc::Copied const&>(node));
        break;
    case doc::Kind::throws:
        writeThrows(dynamic_cast<doc::Throws const&>(node));
        break;
    case doc::Kind::details:
        writeDetails(dynamic_cast<doc::Details const&>(node));
        break;
    case doc::Kind::see:
        writeSee(dynamic_cast<doc::See const&>(node));
        break;
    case doc::Kind::precondition:
        writePrecondition(dynamic_cast<doc::Precondition const&>(node));
        break;
    case doc::Kind::postcondition:
        writePostcondition(dynamic_cast<doc::Postcondition const&>(node));
        break;
    default:
        // unknown kind
        MRDOCS_UNREACHABLE();
    }
}

void
XMLWriter::
writeReference(
    doc::Reference const& node)
{
    tags_.write("reference", node.string, {
        { node.id }
        });
}

void
XMLWriter::
writeCopied(
    doc::Copied const& node)
{
    std::string_view tag_name;
    switch(node.parts)
    {
    case doc::Parts::all:
        tag_name = "copydoc";
        break;
    case doc::Parts::brief:
        tag_name = "copybrief";
        break;
    case doc::Parts::description:
        tag_name = "copydetails";
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    tags_.write(tag_name, node.string, {
        { node.id }
        });
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
writeDetails(
    doc::Details const& para,
    llvm::StringRef tag)
{
    tags_.open("details", {
        { "class", tag, ! tag.empty() }});
    writeNodes(para.children);
    tags_.close("details");
}

void
XMLWriter::
writeSee(
    doc::See const& para,
    llvm::StringRef tag)
{
    tags_.open("see", {
        { "class", tag, ! tag.empty() }});
    writeNodes(para.children);
    tags_.close("see");
}

void
XMLWriter::
writePrecondition(
    doc::Precondition const& para)
{
    tags_.open("pre", {});
    writeNodes(para.children);
    tags_.close("pre");
}

void
XMLWriter::
writePostcondition(
    doc::Postcondition const& para)
{
    tags_.open("post", {});
    writeNodes(para.children);
    tags_.close("post");
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
writeThrows(
    doc::Throws const& throws)
{
    if(throws.empty())
        return;
    tags_.open("throws");
    writeNodes(throws.children);
    tags_.close("throws");
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
