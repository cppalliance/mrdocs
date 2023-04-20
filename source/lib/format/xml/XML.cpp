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

#include "XML.hpp"
#include "format/base64.hpp"
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------
//
// escape
//
//------------------------------------------------

/** Manipulator to apply XML escaping to output.
*/
struct escape
{
    explicit
    escape(
        llvm::StringRef const& s) noexcept
        : s_(s)
    {
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        escape const& t)
    {
        t.write(os);
        return os;
    }

private:
    void write(llvm::raw_ostream& os) const;

    llvm::StringRef s_;
};

void
escape::
write(
    llvm::raw_ostream& os) const
{
    std::size_t pos = 0;
    auto const size = s_.size();
    while(pos < size)
    {
    unescaped:
        auto const found = s_.find_first_of("<>&'\"", pos);
        if(found == llvm::StringRef::npos)
        {
            os.write(s_.data() + pos, s_.size() - pos);
            break;
        }
        os.write(s_.data() + pos, found - pos);
        pos = found;
        while(pos < size)
        {
            auto const c = s_[pos];
            switch(c)
            {
            case '<':
                os.write("&lt;", 4);
                break;
            case '>':
                os.write("&gt;", 4);
                break;
            case '&':
                os.write("&amp;", 5);
                break;
            case '\'':
                os.write("&apos;", 6);
                break;
            case '\"':
                os.write("&quot;", 6);
                break;
            default:
                goto unescaped;
            }
            ++pos;
        }
    }
}

} // (anon)

//------------------------------------------------
//
// XMLGenerator
//
//------------------------------------------------

bool
XMLGenerator::
buildOne(
    llvm::StringRef fileName,
    Corpus& corpus,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;

    std::error_code ec;
    llvm::raw_fd_ostream os(
        fileName,
        ec,
        fs::CD_CreateAlways,
        fs::FA_Write,
        fs::OF_None);
    if(R.error(ec, "open a stream for '", fileName, "'"))
        return false;

    Writer w(os, corpus, R);
    w.write();
    return true;
}

bool
XMLGenerator::
buildString(
    std::string& dest,
    Corpus& corpus,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);

    Writer w(os, corpus, R);
    w.write();
    return true;
}

//------------------------------------------------
//
// Attrs
//
//------------------------------------------------

struct XMLGenerator::Writer::
    Attr
{
    llvm::StringRef name;
    std::string value;
    bool pred;

    Attr(
        llvm::StringRef name_,
        llvm::StringRef value_,
        bool pred_ = true) noexcept
        : name(name_)
        , value(value_)
        , pred(pred_)
    {
    }

    Attr(AccessSpecifier access) noexcept
        : name("access")
        , value(clang::getAccessSpelling(access))
        , pred(access != AccessSpecifier::AS_none)
    {
    }

    Attr(SymbolID USR)
        : name("id")
        , value(toString(USR))
        , pred(USR != EmptySID)
    {
    }
};

XMLGenerator::
Writer::
Attrs::
Attrs(
    std::initializer_list<Attr> init)
    : init_(init)
{
}

llvm::raw_ostream&
operator<<(
    llvm::raw_ostream& os,
    XMLGenerator::Writer::Attrs const& attrs)
{
    for(auto const& attr : attrs.init_)
        if(attr.pred)
            os <<
                ' ' << attr.name << '=' <<
                "\"" << escape(attr.value) << "\"";
    return os;
}

//------------------------------------------------
//
// Writer
//
//------------------------------------------------

XMLGenerator::
Writer::
Writer(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : RecursiveWriter(os, corpus, R)
{
}

void
XMLGenerator::
Writer::
write()
{
    os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
    writeAllSymbols();
    visitNamespace(corpus_.globalNamespace());
    os_ <<
        "</mrdox>\n";
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeAllSymbols()
{
    auto list = makeAllSymbols();
    openTag("symbols");
    for(auto const& I : list)
    {
        writeTag("symbol", {}, {
            { "name", I.fqName },
            { "tag", I.symbolType },
            { I.id } });
    }
    closeTag("symbols");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
visitNamespace(
    NamespaceInfo const& I)
{
    openTag("namespace", {
        { "name", I.Name },
        { I.USR }
        });

    writeInfo(I);
    writeJavadoc(I.javadoc);

    visitScope(I.Children);

    closeTag("namespace");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
visitRecord(
    RecordInfo const& I)
{
    llvm::StringRef tag =
        clang::TypeWithKeyword::getTagTypeKindName(I.TagType);
    openTag(tag, {
        { "name", I.Name },
        { I.USR }
        });

    writeInfo(I);
    writeSymbol(I);
    for(auto const& J : I.Bases)
        writeBaseRecord(J);
    // VFALCO data members?
    for(auto const& J : I.Members)
        writeMemberType(J);
    writeJavadoc(I.javadoc);

    visitScope(I.Children);

    closeTag(tag);
}

//------------------------------------------------

void
XMLGenerator::
Writer::
visitFunction(
    FunctionInfo const& I)
{
    openTag("function", {
        { "name", I.Name },
        { I.Access },
        { I.USR }
        });

    writeInfo(I);
    writeSymbol(I);
    writeReturnType(I.ReturnType);
    for(auto const& J : I.Params)
        writeParam(J);
    if(I.Template)
        for(TemplateParamInfo const& J : I.Template->Params)
            writeTemplateParam(J);
    writeJavadoc(I.javadoc);

    closeTag("function");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
visitTypedef(
    TypedefInfo const& I)
{
    openTag("typedef", {
        { "name", I.Name },
        { I.USR }
        });

    writeInfo(I);
    writeSymbol(I);
    if(I.Underlying.Type.USR != EmptySID)
        writeTag("qualusr", toBase64(I.Underlying.Type.USR));
    writeJavadoc(I.javadoc);

    closeTag("typedef");
}

void
XMLGenerator::
Writer::
visitEnum(
    EnumInfo const& I)
{
    openTag("enum", {
        { "name", I.Name },
        { I.USR }
        });

    writeInfo(I);
    for(auto const& v : I.Members)
        writeTag("element", {}, {
            { "name", v.Name },
            { "value", v.Value },
            });
    writeJavadoc(I.javadoc);

    closeTag("enum");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeInfo(
    Info const& I)
{
#if 0
    std::string temp;
    indent() << "<path>" << I.Path << "</path>\n";
    indent() << "<ename>" << I.extractName() << "</ename>\n";
    indent() << "<fqn>" << I.getFullyQualifiedName(temp) << "</fqn>\n";
#endif
}

void
XMLGenerator::
Writer::
writeSymbol(
    SymbolInfo const& I)
{
    if(I.DefLoc)
        writeLocation(*I.DefLoc, true);
    for(auto const& loc : I.Loc)
        writeLocation(loc, false);
}

void
XMLGenerator::
Writer::
writeLocation(
    Location const& loc,
    bool def)
{
    writeTag("file", {}, {
        { "path", loc.Filename },
        { "line", std::to_string(loc.LineNumber) },
        { "class", "def", def } });
}

void
XMLGenerator::
Writer::
writeBaseRecord(
    BaseRecordInfo const& I)
{
    writeTag("base", {}, {
        { "name", I.Name },
        { I.Access },
        { "modifier", "virtual", I.IsVirtual },
        { I.USR } });
    if(! corpus_.exists(I.USR))
    {
        // e,g. for std::true_type
        return;
    }
}

void
XMLGenerator::
Writer::
writeParam(
    FieldTypeInfo const& I)
{
    writeTag("param", {}, {
        { "name", I.Name, ! I.Name.empty() },
        { "default", I.DefaultValue, ! I.DefaultValue.empty() },
        { "type", I.Type.Name },
        { I.Type.USR } });
}

void
XMLGenerator::
Writer::
writeTemplateParam(
    TemplateParamInfo const& I)
{
    writeTag("tparam", {}, {
        { "decl", I.Contents }
        });
}

void
XMLGenerator::
Writer::
writeMemberType(
    MemberTypeInfo const& I)
{
    writeTag("data", {}, {
        { "name", I.Name },
        { "type", I.Type.Name },
        { "value", I.DefaultValue, ! I.DefaultValue.empty() },
        { I.Access },
        { I.Type.USR } });
}

void
XMLGenerator::
Writer::
writeReturnType(
    TypeInfo const& I)
{
    if(I.Type.Name == "void")
        return;
    writeTag("return", {}, {
        { "name", I.Type.Name },
        { I.Type.USR }
        });
}

//------------------------------------------------

void
XMLGenerator::
Writer::
writeJavadoc(Javadoc const& jd)
{
    if(jd.empty())
        return;
    openTag("doc");
    if(auto brief = jd.getBrief())
        writeBrief(brief);
    writeNodes(jd.getBlocks());
    writeReturns(jd.getReturns());
    writeNodes(jd.getParams());
    writeNodes(jd.getTParams());
    closeTag("doc");
}

template<class T>
void
XMLGenerator::
Writer::
writeNodes(
    List<T> const& list)
{
    if(list.empty())
        return;
    for(auto const& node : list)
        writeNode(node);
}

void
XMLGenerator::
Writer::
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
    case Javadoc::Kind::admonition:
        writeAdmonition(static_cast<Javadoc::Admonition const&>(node));
        break;
    case Javadoc::Kind::code:
        writeCode(static_cast<Javadoc::Code const&>(node));
        break;
    case Javadoc::Kind::param:
        writeParam(static_cast<Javadoc::Param const&>(node));
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
XMLGenerator::
Writer::
writeBrief(
    Javadoc::Paragraph const* node)
{
    if(! node)
        return;
    if(node->empty())
        return;
    openTag("brief");
    writeNodes(node->children);
    closeTag("brief");
}

void
XMLGenerator::
Writer::
writeText(
    Javadoc::Text const& node)
{
    indent() <<
        "<text>" <<
        escape(node.string) <<
        "</text>\n";
}

void
XMLGenerator::
Writer::
writeStyledText(
    Javadoc::StyledText const& node)
{
    writeTag(toString(node.style), node.string);
}

void
XMLGenerator::
Writer::
writeParagraph(
    Javadoc::Paragraph const& para,
    llvm::StringRef tag)
{
    openTag("para", {
        { "class", tag, ! tag.empty() }});
    writeNodes(para.children);
    closeTag("para");
}

void
XMLGenerator::
Writer::
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
XMLGenerator::
Writer::
writeCode(Javadoc::Code const& code)
{
    if(code.children.empty())
    {
        indent() << "<code/>\n";
        return;
    }

    openTag("code");
    writeNodes(code.children);
    closeTag("code");
}

void
XMLGenerator::
Writer::
writeReturns(
    Javadoc::Returns const& returns)
{
    if(returns.empty())
        return;
    openTag("returns");
    writeNodes(returns.children);
    closeTag("returns");
}

void
XMLGenerator::
Writer::
writeParam(
    Javadoc::Param const& param)
{
    openTag("param", {
        { "name", param.name, ! param.name.empty() }});
    writeNodes(param.children);
    closeTag("param");
}

void
XMLGenerator::
Writer::
writeTParam(
    Javadoc::TParam const& tparam)
{
    openTag("tparam", {
        { "name", tparam.name, ! tparam.name.empty() }});
    writeNodes(tparam.children);
    closeTag("tparam");
}

//------------------------------------------------

void
XMLGenerator::
Writer::
openTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    indent() << '<' << tag << attrs << ">\n";
    adjustNesting(1);
}

void
XMLGenerator::
Writer::
closeTag(
    llvm::StringRef tag)
{
    adjustNesting(-1);
    indent() << "</" << tag << ">\n";
}

void
XMLGenerator::
Writer::
writeTag(
    llvm::StringRef tag,
    llvm::StringRef value,
    Attrs attrs)
{
    if(value.empty())
    {
        indent() << "<" << tag << attrs << "/>\n";
        return;
    }

    indent() <<
        "<" << tag << attrs << ">" <<
        escape(value) <<
        "</" << tag << ">\n";
}

//------------------------------------------------

std::string
XMLGenerator::
Writer::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
XMLGenerator::
Writer::
toString(
    InfoType it) noexcept
{
    switch(it)
    {
    case InfoType::IT_default:    return "default";
    case InfoType::IT_namespace:  return "namespace";
    case InfoType::IT_record:     return "record";
    case InfoType::IT_function:   return "function";
    case InfoType::IT_enum:       return "enum";
    case InfoType::IT_typedef:    return "typedef";
    default:
        llvm_unreachable("unknown InfoType");
    }
}

llvm::StringRef
XMLGenerator::
Writer::
toString(
    Javadoc::Style style) noexcept
{
    llvm::StringRef s;
    switch(style)
    {
    case Javadoc::Style::bold: return "bold";
    case Javadoc::Style::mono: return "mono";
    case Javadoc::Style::italic: return "italic";

    // should never get here
    case Javadoc::Style::none: return "";

    default:
        llvm_unreachable("unknown style");
    }
}

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<XMLGenerator>();
}

} // mrdox
} // clang
