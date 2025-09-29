//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLWriter.hpp"
#include <mrdocs/Platform.hpp>
#include "CXXTags.hpp"
#include <lib/Support/LegibleNames.hpp>
#include <lib/Support/Radix.hpp>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

namespace mrdocs::xml {

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
    os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<mrdocs xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdocs/raw/develop/mrdocs.rnc\">\n";

    this->operator()(corpus_.globalNamespace());

    os_ << "</mrdocs>\n";

    return {};
}

template <std::derived_from<Symbol> SymbolTy>
void
XMLWriter::
operator()(SymbolTy const& I)
{
    if (Symbol const& base = I;
        base.Extraction == ExtractionMode::Dependency)
    {
        return;
    }
    #define INFO(Type) if constexpr(SymbolTy::is##Type()) write##Type(I);
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
}

//------------------------------------------------

void
XMLWriter::
writeNamespace(
    NamespaceSymbol const& I)
{
    constexpr std::string_view namespaceTagName = "namespace";
    tags_.open(namespaceTagName, {
        { "name", I.Name, ! I.Name.empty() },
        { I.id },
        { "is-anonymous", "1", I.IsAnonymous},
        { "is-inline", "1", I.IsInline}
    });
    writeDocComment(I.doc);
    for (Name const& NI: I.UsingDirectives)
    {
        if (auto const& id = NI.id; id != SymbolID::invalid)
        {
            tags_.write("using-directive", {}, { { id } });
        }
    }
    corpus_.traverse(I, *this);
    tags_.close(namespaceTagName);
}

void
XMLWriter::
writeEnum(
    EnumSymbol const& I)
{
    constexpr std::string_view enumTagName = "enum";
    tags_.open(enumTagName, {
        { "name", I.Name },
        { "class", "scoped", I.Scoped },
        { I.Access },
        { I.id }
    });
    if (I.UnderlyingType)
    {
        tags_.open(baseTagName);
        writeType(I.UnderlyingType, tags_);
        tags_.close(baseTagName);
    }

    writeSourceInfo(I.Loc);

    writeDocComment(I.doc);

    corpus_.traverse(I, *this);

    tags_.close(enumTagName);
}

void
XMLWriter::
writeEnumConstant(
    EnumConstantSymbol const& I)
{
    std::string val = I.Initializer.Value ?
        std::to_string(*I.Initializer.Value) :
        I.Initializer.Written;

    constexpr std::string_view enumConstantTagName = "enum-constant";
    tags_.open(enumConstantTagName, {
        { "name", I.Name },
        { "initializer", val },
        { I.Access },
        { I.id }
    });

    writeSourceInfo(I.Loc);

    writeDocComment(I.doc);

    tags_.close(enumConstantTagName);
}

void
XMLWriter::
writeFriend(
    FriendInfo const& I)
{
    constexpr std::string_view friendTagName = "friend";
    tags_.open(friendTagName, {
        { I.id }
        });

    Attributes attrs = {};
    if (I.id)
    {
        attrs.push({ I.id });
    }
    else if (I.Type)
    {
        attrs.push({ "type", toString(**I.Type) });
    }

    tags_.write("befriended", {}, attrs);

    tags_.close(friendTagName);
}

void
XMLWriter::
writeFunction(
    FunctionSymbol const& I)
{
    openTemplate(I.Template);

    auto except_spec = toString(I.Noexcept);
    auto explicit_spec = toString(I.Explicit);

    constexpr std::string_view functionTagName = "function";
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

    writeSourceInfo(I.Loc);

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

    MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
    writeReturnType(*I.ReturnType, tags_);

    for (auto const& J: I.Params)
    {
        ::mrdocs::xml::writeParam(J, tags_);
    }

    writeDocComment(I.doc);

    tags_.close(functionTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeOverloads(
    OverloadsSymbol const& I)
{
    corpus_.traverse(I, *this);
}

void
XMLWriter::
writeGuide(
    GuideSymbol const& I)
{
    openTemplate(I.Template);

    auto explicit_spec = toString(I.Explicit);

    constexpr std::string_view guideTagName = "guide";
    tags_.open(guideTagName, {
        { "name", I.Name },
        { I.Access },
        { "explicit-spec", explicit_spec,
            ! explicit_spec.empty() },
        { I.id }
        });

    writeSourceInfo(I.Loc);

    tags_.open(deducedTagName);
    writeType(I.Deduced, tags_);
    tags_.close(deducedTagName);

    for (auto const& J: I.Params)
    {
        xml::writeParam(J, tags_);
    }

    writeDocComment(I.doc);

    tags_.close(guideTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeConcept(
    ConceptSymbol const& I)
{
    openTemplate(I.Template);

    constexpr std::string_view conceptTagName = "concept";
    tags_.open(conceptTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id },
        { "constraint", I.Constraint.Written },
        });

    writeSourceInfo(I.Loc);

    writeDocComment(I.doc);

    tags_.close(conceptTagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeNamespaceAlias(
    NamespaceAliasSymbol const& I)
{
    constexpr std::string_view namespaceAliasTagName = "namespace-alias";
    tags_.open(namespaceAliasTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I.Loc);

    writeDocComment(I.doc);

    tags_.write("aliased", {}, {
        {"name", toString(I.AliasedSymbol)},
        { I.AliasedSymbol.id }
    });

    tags_.close(namespaceAliasTagName);
}

void
XMLWriter::
    writeUsing(UsingSymbol const& I)
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
    MRDOCS_ASSERT(!I.IntroducedName.valueless_after_move());
    qualifierStr = toString(*I.IntroducedName);

    constexpr std::string_view usingTagName = "using";
    tags_.open(usingTagName, {
        { I.Access },
        { I.id },
        { "class", classStr, ! classStr.empty() },
        { "qualifier", qualifierStr, !qualifierStr.empty() }
    });

    writeSourceInfo(I.Loc);

    writeDocComment(I.doc);

    for (auto const& id : I.ShadowDeclarations)
        tags_.write("named", {}, { id });

    tags_.close(usingTagName);
}

void
XMLWriter::
writeRecord(
    RecordSymbol const& I)
{
    openTemplate(I.Template);

    auto tagName = toString(I.KeyKind);

    tags_.open(tagName, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I.Loc);

    writeAttr(I.IsFinal, "is-final", tags_);
    writeAttr(I.IsFinalDestructor, "is-final-dtor", tags_);

    for (auto const& B : I.Bases)
    {
        tags_.open(baseTagName, {
            { B.Access },
            { "class", "virtual", B.IsVirtual },
        });
        writeType(*B.Type, tags_);
        tags_.close(baseTagName);
    }

    for (FriendInfo const& F : I.Friends)
    {
        writeFriend(F);
    }

    writeDocComment(I.doc);

    corpus_.traverse(I, *this);

    tags_.close(tagName);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeTypedef(
    TypedefSymbol const& I)
{
    openTemplate(I.Template);

    constexpr std::string_view namespaceAliasTagName = "namespace-alias";
    constexpr std::string_view typedefTagName = "typedef";
    llvm::StringRef tag;
    if (I.IsUsing)
    {
        tag = namespaceAliasTagName;
    } else
    {
        tag = typedefTagName;
    }
    tags_.open(tag, {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeSourceInfo(I.Loc);

    writeType(I.Type, tags_);

    writeDocComment(I.doc);

    tags_.close(tag);

    closeTemplate(I.Template);
}

void
XMLWriter::
writeVariable(
    VariableSymbol const& I)
{
    openTemplate(I.Template);

    std::string bit_width;
    if(I.IsBitfield)
    {
        bit_width = I.BitfieldWidth.Value ?
            std::to_string(*I.BitfieldWidth.Value) :
            I.BitfieldWidth.Written;
    }

    tags_.open(varTagName, {
        { "name", I.Name },
        { I.Access },
        { I.id },
           { "width", bit_width, I.IsBitfield },
        { "default", I.Initializer.Written, ! I.Initializer.Written.empty() }
        });

    writeSourceInfo(I.Loc);

    if(I.IsMutable)
        tags_.write(attributeTagName, {}, {
            {"id", "is-mutable"}
        });

    writeAttr(I.StorageClass, "storage-class", tags_);
    writeAttr(I.IsInline, "is-inline", tags_);
    writeAttr(I.IsConstexpr, "is-constexpr", tags_);
    writeAttr(I.IsConstinit, "is-constinit", tags_);
    writeAttr(I.IsThreadLocal, "is-thread-local", tags_);

    // writeAttr(I.IsVariant, "is-variant", tags_);
    // writeAttr(I.IsMaybeUnused, "maybe-unused", tags_);
    // writeAttr(I.IsDeprecated, "deprecated", tags_);
    // writeAttr(I.HasNoUniqueAddress, "no-unique-address", tags_);

    writeType(I.Type, tags_);

    writeDocComment(I.doc);

    tags_.close(varTagName);

    closeTemplate(I.Template);
}

//------------------------------------------------

void
XMLWriter::
writeSourceInfo(
    SourceInfo const& I)
{
    if (I.DefLoc)
    {
        writeLocation(*I.DefLoc, true);
    }
    for (auto const& loc: I.Loc)
    {
        writeLocation(loc, false);
    }
}

void
XMLWriter::
writeLocation(
    Location const& loc,
    bool def)
{
    tags_.write("file", {}, {
        // { "full-path", loc.FullPath },
        { "short-path", loc.ShortPath },
        { "source-path", loc.SourcePath },
        { "line", std::to_string(loc.LineNumber) },
        { "class", "def", def }});
}

//------------------------------------------------

void
XMLWriter::
openTemplate(
    Optional<TemplateInfo> const& I)
{
    if (!I)
    {
        return;
    }

    tags_.open(templateTagName, {
        {"class", toString(I->specializationKind()),
            I->specializationKind() != TemplateSpecKind::Primary},
        {"requires", I->Requires.Written,
            ! I->Requires.Written.empty()},
        {I->Primary}
    });

    for (auto const& tparam: I->Params)
    {
        writeTemplateParam(*tparam, tags_);
    }
    for (auto const& targ: I->Args)
    {
        writeTemplateArg(*targ, tags_);
    }
}

void
XMLWriter::
closeTemplate(
    const Optional<TemplateInfo>& I)
{
    if(! I)
        return;
    tags_.close(templateTagName);
}

//------------------------------------------------

void
XMLWriter::
writeDocComment(
    Optional<DocComment> const& doc)
{
    if (!doc)
    {
        return;
    }
    tags_.open(docTagName);
    if (doc->brief)
    {
        writeBrief(*doc->brief);
    }
    writeBlocks(doc->Document);
    writeBlocks(doc->returns);
    writeBlocks(doc->params);
    writeBlocks(doc->tparams);
    writeBlocks(doc->exceptions);
    writeBlocks(doc->sees);
    writeBlocks(doc->preconditions);
    writeBlocks(doc->postconditions);
    if (!doc->relates.empty())
    {
        tags_.open(relatesTagName);
        writeInlines(doc->relates);
        tags_.close(relatesTagName);
    }
    if (!doc->related.empty())
    {
        tags_.open(relatedTagName);
        writeInlines(doc->related);
        tags_.close(relatedTagName);
    }
    tags_.close(docTagName);
}


//void
//XMLWriter::
//writeNode(doc::Node const& node)
//{
//    auto* nodePtr = &node;
//    auto* blockPtr = dynamic_cast<doc::Block const*>(nodePtr);
//    if (blockPtr)
//    {
//        switch (blockPtr->Kind)
//        {
//            #define INFO(Type) case doc::BlockKind::Type: \
//                write##Type(dynamic_cast<doc::Type##Block const&>(node)); \
//                return;
//#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
//        default:
//            MRDOCS_UNREACHABLE();
//        }
//    }
//    auto* inlinePtr = dynamic_cast<doc::Inline const*>(nodePtr);
//    if (inlinePtr)
//    {
//        switch (inlinePtr->Kind)
//        {
//            #define INFO(Type) case doc::InlineKind::Type: \
//                write##Type(dynamic_cast<doc::Type##Inline const&>(node)); \
//                return;
//#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
//        default:
//            MRDOCS_UNREACHABLE();
//        }
//    }
//    MRDOCS_UNREACHABLE();
//}

void
XMLWriter::
writeReference(
    doc::ReferenceInline const& node)
{
    tags_.write("reference", node.literal, {
        { node.id }
        });
}

void
XMLWriter::
writeCopyDetails(
    doc::CopyDetailsInline const& node)
{
    std::string_view constexpr tag_name = "copydetails";
    tags_.write(tag_name, node.string, {
        { node.id }
        });
}

void
XMLWriter::
writeLink(
    doc::LinkInline const& node)
{

    tags_.write(
        "link", doc::getAsPlainText(node.asInline()),
        {{ "href", node.href }});
}

void
XMLWriter::
writeListItem(
    doc::ListItem const& node)
{
    tags_.open("listitem");
    // flatten blocks to write their inlines directly when possible
    // this maintains the schema, but it would be best to
    // just render the blocks directly
    for (auto const& childb : node.blocks)
    {
        auto* ilnPtr = dynamic_cast<doc::InlineContainer const*>(&*childb);
        MRDOCS_CHECK_OR_CONTINUE(ilnPtr);
        writeInlines(ilnPtr->children);
    }
    tags_.close("listitem");
}

void
XMLWriter::
writeList(
    doc::ListBlock const& node)
{
    tags_.open("unorderedlist");
    for (auto const& li: node.items)
    {
        writeListItem(li);
    }
    tags_.close("unorderedlist");
}

void
XMLWriter::
writeBrief(
    doc::BriefBlock const& node)
{
    tags_.open("brief");
    writeInlines(node.children);
    tags_.close("brief");
}

void
XMLWriter::
writeText(
    doc::TextInline const& node)
{
    tags_.indent() <<
        "<text>" <<
        xmlEscape(node.literal) <<
        "</text>\n";
}

void
XMLWriter::
writeCode(doc::CodeInline const& node)
{
    tags_.write("mono", doc::getAsPlainText(node.asInline()));
}

void
XMLWriter::
writeStrong(doc::StrongInline const& node)
{
    tags_.write("bold", doc::getAsPlainText(node.asInline()));
}

void
XMLWriter::
writeEmph(doc::EmphInline const& node)
{
    tags_.write("italic", doc::getAsPlainText(node.asInline()));
}

void
XMLWriter::
writeHeading(
    doc::HeadingBlock const& heading)
{
    tags_.write("head", doc::getAsPlainText(heading.asInlineContainer()));
}

void
XMLWriter::
writeInlineContainer(
    doc::InlineContainer const& node,
    std::string_view tag)
{
    tags_.open("para", {{ "class", tag, !tag.empty() }});
    writeInlines(node.children);
    tags_.close("para");
}

void
XMLWriter::
writeSee(doc::SeeBlock const& para)
{
    tags_.open("see");
    writeInlines(para.children);
    tags_.close("see");
}

void
XMLWriter::
writePrecondition(
    doc::PreconditionBlock const& para)
{
    tags_.open("pre", {});
    writeInlines(para.children);
    tags_.close("pre");
}

void
XMLWriter::
writePostcondition(
    doc::PostconditionBlock const& para)
{
    tags_.open("post", {});
    writeInlines(para.children);
    tags_.close("post");
}

void
XMLWriter::
writeAdmonition(
    doc::AdmonitionBlock const& admonition)
{
    llvm::StringRef tag;
    switch(admonition.admonish)
    {
    case doc::AdmonitionKind::note:
        tag = "note";
        break;
    case doc::AdmonitionKind::tip:
        tag = "tip";
        break;
    case doc::AdmonitionKind::important:
        tag = "important";
        break;
    case doc::AdmonitionKind::caution:
        tag = "caution";
        break;
    case doc::AdmonitionKind::warning:
        tag = "warning";
        break;
    default:
        // unknown style
        MRDOCS_UNREACHABLE();
    }
    MRDOCS_CHECK_OR(!admonition.blocks.empty());
    auto& firstBlock = admonition.blocks.front();
    auto const* asInlineContainer = dynamic_cast<doc::InlineContainer const*>(&*firstBlock);
    if (asInlineContainer)
    {
        writeInlineContainer(*asInlineContainer, tag);
    }
}

void
XMLWriter::
writeCode(doc::CodeBlock const& code)
{
    if(code.literal.empty())
    {
        tags_.indent() << "<code/>\n";
        return;
    }

    tags_.open("code");
    tags_.indent() << "<text>" << xmlEscape(code.literal) << "</text>\n";
    tags_.close("code");
}

void
XMLWriter::
writeReturns(
    doc::ReturnsBlock const& returns)
{
    if (returns.empty())
    {
        return;
    }
    tags_.open("returns");
    writeInlines(returns.children);
    tags_.close("returns");
}

void
XMLWriter::
writeThrows(
    doc::ThrowsBlock const& throws)
{
    if(throws.empty())
        return;
    tags_.open("throws");
    writeInlines(throws.children);
    tags_.close("throws");
}

void
XMLWriter::
writeParam(doc::ParamBlock const& param)
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
    writeInlines(param.children);
    tags_.close("param");
}

void
XMLWriter::
writeTParam(
    doc::TParamBlock const& tparam)
{
    tags_.open("tparam", {
        { "name", tparam.name, ! tparam.name.empty() }});
    writeInlines(tparam.children);
    tags_.close("tparam");
}

void
XMLWriter::
writeImage(doc::ImageInline const& el)
{
    tags_.open("image");
    auto text = doc::getAsPlainText(el.asInline());
    if (!text.empty())
    {
        tags_.write("alt", text);
    }
    tags_.close("image");
}

void
XMLWriter::
writeQuote(doc::QuoteBlock const& el)
{
    tags_.open("quote");
    writeBlocks(el.blocks);
    tags_.close("quote");
}

void
XMLWriter::
writeTable(doc::TableBlock const& el)
{
    tags_.open("table");
    for (auto const& row : el.items)
    {
        tags_.open("tablerow");
        for (auto const& cell : row.Cells)
        {
            tags_.open("tablecell");
            auto str = doc::getAsPlainText(cell.asInlineContainer());
            if (!str.empty())
            {
                tags_.write("celltext",  str);
            }
            tags_.close("tablecell");
        }
        tags_.close("tablerow");
    }
    tags_.close("table");
}

void
XMLWriter::
writeHighlight(doc::HighlightInline const& el)
{
    tags_.open("highlight");
    auto text = doc::getAsPlainText(el.asInline());
    tags_.close("highlight");
}

void
XMLWriter::
writeLineBreak(doc::LineBreakInline const& el)
{
    tags_.write("linebreak", {});
}

void
XMLWriter::
writeParagraph(doc::ParagraphBlock const& el)
{
    tags_.open("para");
    for (Polymorphic<doc::Inline> const& child : el.children)
    {
        writeInline(*child);
    }
    tags_.close("para");
}

void
XMLWriter::
writeSoftBreak(doc::SoftBreakInline const& el)
{
    tags_.write("softbreak", {});
}

void
XMLWriter::
writeSubscript(doc::SubscriptInline const& el)
{
    tags_.open("subscript");
    auto text = doc::getAsPlainText(el.asInline());
    if (!text.empty())
    {
        tags_.indent() << xmlEscape(text) << "\n";
    }
    tags_.close("subscript");
}

void
XMLWriter::
writeSuperscript(doc::SuperscriptInline const& el)
{
    tags_.open("superscript");
    auto text = doc::getAsPlainText(el.asInline());
    if (!text.empty())
    {
        tags_.indent() << xmlEscape(text) << "\n";
    }
    tags_.close("superscript");
}

void
XMLWriter::
writeStrikethrough(doc::StrikethroughInline const& el)
{
    tags_.open("strikethrough");
    auto text = doc::getAsPlainText(el.asInline());
    if (!text.empty())
    {
        tags_.indent() << xmlEscape(text) << "\n";
    }
    tags_.close("strikethrough");
}

void
XMLWriter::
writeThematicBreak(doc::ThematicBreakBlock const& el)
{
    tags_.write("thematicbreak", {});
}

void
XMLWriter::
writeFootnoteReference(doc::FootnoteReferenceInline const& el)
{
    tags_.open("footnotereference");
    if (!el.label.empty())
    {
        tags_.write("label", el.label);
    }
    tags_.close("footnotereference");
}

void
XMLWriter::
writeFootnoteDefinition(doc::FootnoteDefinitionBlock const& el)
{
    tags_.open("footnotedefinition");
    if (!el.label.empty())
    {
        tags_.write("label", el.label);
    }
    writeBlocks(el.blocks);
    tags_.close("footnotedefinition");
}

void
XMLWriter::
writeMath(doc::MathInline const& el)
{
    tags_.open("math");
    if (!el.literal.empty())
    {
        tags_.indent() << xmlEscape(el.literal) << "\n";
    }
    tags_.close("math");
}

void
XMLWriter::
writeMath(doc::MathBlock const& el)
{
    tags_.open("mathblock");
    if (!el.literal.empty())
    {
        tags_.indent() << xmlEscape(el.literal) << "\n";
    }
    tags_.close("mathblock");
}

void
XMLWriter::
writeDefinitionList(doc::DefinitionListBlock const& el)
{
    tags_.open("definitionlist");
    for (auto const& item : el.items)
    {
        tags_.open("definitionitem");
        tags_.open("term");
        writeInlines(item.term.children);
        tags_.close("term");
        tags_.open("definition");
        writeBlocks(item.blocks);
        tags_.close("definition");
        tags_.close("definitionitem");
    }
    tags_.close("definitionlist");
}

void
XMLWriter::
writeBlock(doc::Block const& node)
{
    switch (node.Kind)
    {
        #define INFO(Type) case doc::BlockKind::Type: \
            write##Type(node.as##Type()); \
            return;
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
    }
}

void
XMLWriter::
writeInline(doc::Inline const& node)
{
    switch (node.Kind)
    {
        #define INFO(Type) case doc::InlineKind::Type: \
            write##Type(node.as##Type()); \
            return;
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
    }
}


} // mrdocs::xml
