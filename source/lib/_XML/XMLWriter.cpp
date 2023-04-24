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
#include "Support/Radix.hpp"

namespace clang {
namespace mrdox {
namespace xml {

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

//------------------------------------------------
//
// AllSymbol
//
//------------------------------------------------

XMLWriter::
AllSymbol::
AllSymbol(
    Info const& I)
{
    I.getFullyQualifiedName(fqName);
    symbolType = I.symbolType();
    id = I.id;
}

//------------------------------------------------
//
// Attrs
//
//------------------------------------------------

struct XMLWriter::Attr
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

    Attr(SymbolID id)
        : name("id")
        , value(toString(id))
        , pred(id != EmptySID)
    {
    }

    Attr(llvm::Optional<TypeInfo> const& opt)
        : name("type")
        , value(opt ? opt->Type.Name.str() : std::string())
        , pred(opt)
    {
    }
};

//------------------------------------------------
//
// maybe_indent
//
//------------------------------------------------

struct XMLWriter::
    maybe_indent_type
{
    llvm::raw_ostream& os_;
    std::string const& indent_;
    bool indented_ = false;

    maybe_indent_type(
        llvm::raw_ostream& os,
        std::string const& indent) noexcept
        : os_(os)
        , indent_(indent)
    {
    }

    template<class T>
    llvm::raw_ostream&
    operator<<(T const& t)
    {
        if(! indented_)
        {
            os_ << indent_;
            indented_ = true;
        }
        return os_ << t;
    }

    void
    finish()
    {
        if(indented_)
            os_ << '\n';
    }
};

auto
XMLWriter::
maybe_indent() noexcept ->
    maybe_indent_type
{
    return maybe_indent_type(os_, indentString_);
}

//------------------------------------------------

XMLWriter::
Attrs::
Attrs(
    std::initializer_list<Attr> init)
    : init_(init)
{
}

llvm::raw_ostream&
operator<<(
    llvm::raw_ostream& os,
    XMLWriter::Attrs const& attrs)
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
// XMLWriter
//
//------------------------------------------------

XMLWriter::
XMLWriter(
    llvm::raw_ostream& os,
    llvm::raw_fd_ostream* fd_os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : os_(os)
    , fd_os_(fd_os)
    , corpus_(corpus)
    , R_(R)
{
}

llvm::Error
XMLWriter::
build()
{
    os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";

    // VFALCO Do we even need this?
    //writeAllSymbols();

    if(! corpus_.visit(globalNamespaceID, *this))
    {
        if(fd_os_ && fd_os_->error())
            return makeError("raw_fd_ostream returned ", fd_os_->error());
        return makeError("visit failed");
    }
    os_ <<
        "</mrdox>\n";
    return llvm::Error::success();
}

//------------------------------------------------

void
XMLWriter::
writeAllSymbols()
{
    std::vector<AllSymbol> list;
    list.reserve(corpus_.allSymbols().size());
    for(auto const& id : corpus_.allSymbols())
        list.emplace_back(corpus_.get<Info>(id));

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

bool
XMLWriter::
visit(
    NamespaceInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    openTag("namespace", {
        { "name", I.Name },
        { I.id }
        });

    writeInfo(I);
    writeJavadoc(I.javadoc);

    if(! corpus_.visit(I.Children, *this))
        return false;

    closeTag("namespace");

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    RecordInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    llvm::StringRef tag =
        clang::TypeWithKeyword::getTagTypeKindName(I.TagType);
    openTag(tag, {
        { "name", I.Name },
        { I.id }
        });

    writeInfo(I);
    writeSymbol(I);
    for(auto const& J : I.Bases)
        writeBaseRecord(J);
    // VFALCO data members?
    for(auto const& J : I.Members)
        writeMemberType(J);

    // Friends
    for(auto const& id : I.Friends)
        writeTag("friend", "", { { id } });

    writeJavadoc(I.javadoc);

    if(! corpus_.visit(I.Children, *this))
        return false;

    closeTag(tag);

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    FunctionInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    openTag("function", {
        { "name", I.Name },
        { I.Access },
        { I.id }
        });

    writeInfo(I);
    writeSymbol(I);
    {
        // FunctionInfo::Specs
        auto os = maybe_indent();

        switch(I.specs.storageClass())
        {
        case SC_None:
            break;
        case SC_Extern:
            os << "<extern/>";
            break;
        case SC_Static:
            os << "<static/>";
            break;
        case SC_PrivateExtern:
            // VFALCO What is this for?
            os << "<pextern/>";
            break;
        default:
            Assert(isLegalForFunction(I.specs.storageClass()));
            break;
        }
        switch(I.specs.refQualifier())
        {
        case RQ_None:
            break;
        case RQ_LValue:
            os << "<lvref/>";
            break;
        case RQ_RValue:
            os << "<rvref/>";
            break;
        }
        if(I.specs.isSet(FunctionInfo::constBit))
            os << "<const/>";
        if(I.specs.isSet(FunctionInfo::constevalBit))
            os << "<consteval/>";
        if(I.specs.isSet(FunctionInfo::constexprBit))
            os << "<constexpr/>";
        if(I.specs.isSet(FunctionInfo::inlineBit))
            os << "<inline/>";
        if(I.specs.isSet(FunctionInfo::noexceptBit))
            os << "<noexcept/>";
        if(I.specs.isSet(FunctionInfo::noreturnBit))
            os << "<noreturn/>";
        if(I.specs.isSet(FunctionInfo::overrideBit))
            os << "<override/>";
        if(I.specs.isSet(FunctionInfo::pureBit))
            os << "<pure/>";
        if(I.specs.isSet(FunctionInfo::specialBit))
            os << "<special/>";
        if(I.specs.isSet(FunctionInfo::trailReturnBit))
            os << "<trailing/>";
        if(I.specs.isSet(FunctionInfo::variadicBit))
            os << "<variadic/>";
        if(I.specs.isSet(FunctionInfo::virtualBit))
            os << "<virtual/>";
        if(I.specs.isSet(FunctionInfo::volatileBit))
            os << "<volatile/>";
        os.finish();
    }
    writeReturnType(I.ReturnType);
    for(auto const& J : I.Params)
        writeParam(J);
    if(I.Template)
        for(TemplateParamInfo const& J : I.Template->Params)
            writeTemplateParam(J);
    writeJavadoc(I.javadoc);

    closeTag("function");

    return true;
}

//------------------------------------------------

bool
XMLWriter::
visit(
    TypedefInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    openTag("typedef", {
        { "name", I.Name },
        { I.id }
        });

    writeInfo(I);
    writeSymbol(I);
    if(I.Underlying.Type.id != EmptySID)
        writeTag("qualusr", toBase64(I.Underlying.Type.id));
    writeJavadoc(I.javadoc);

    closeTag("typedef");

    return true;
}

bool
XMLWriter::
visit(
    EnumInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    openTag("enum", {
        { "name", I.Name },
        { "class", "scoped", I.Scoped },
        { I.BaseType },
        { I.id }
        });

    writeInfo(I);
    writeSymbol(I);
    for(auto const& v : I.Members)
        writeTag("element", {}, {
            { "name", v.Name },
            { "value", v.Value },
            });
    writeJavadoc(I.javadoc);

    closeTag("enum");

    return true;
}

//------------------------------------------------

void
XMLWriter::
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
    writeTag("file", {}, {
        { "path", loc.Filename },
        { "line", std::to_string(loc.LineNumber) },
        { "class", "def", def } });
}

void
XMLWriter::
writeBaseRecord(
    BaseRecordInfo const& I)
{
    writeTag("base", {}, {
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
writeParam(
    FieldTypeInfo const& I)
{
    writeTag("param", {}, {
        { "name", I.Name, ! I.Name.empty() },
        { "default", I.DefaultValue, ! I.DefaultValue.empty() },
        { "type", I.Type.Name },
        { I.Type.id } });
}

void
XMLWriter::
writeTemplateParam(
    TemplateParamInfo const& I)
{
    writeTag("tparam", {}, {
        { "decl", I.Contents }
        });
}

void
XMLWriter::
writeMemberType(
    MemberTypeInfo const& I)
{
    writeTag("data", {}, {
        { "name", I.Name },
        { "type", I.Type.Name },
        { "value", I.DefaultValue, ! I.DefaultValue.empty() },
        { I.Access },
        { I.Type.id } });
}

void
XMLWriter::
writeReturnType(
    TypeInfo const& I)
{
    if(I.Type.Name == "void")
        return;
    writeTag("return", {}, {
        { "name", I.Type.Name },
        { I.Type.id }
        });
}

//------------------------------------------------

void
XMLWriter::
writeJavadoc(
    llvm::Optional<Javadoc> const& javadoc)
{
    if(! javadoc.has_value())
        return;
    openTag("doc");
    if(auto brief = javadoc->getBrief())
        writeBrief(*brief);
    writeNodes(javadoc->getBlocks());
    closeTag("doc");
}

template<class T>
void
XMLWriter::
writeNodes(
    List<T> const& list)
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
XMLWriter::
writeBrief(
    Javadoc::Paragraph const& node)
{
    openTag("brief");
    writeNodes(node.children);
    closeTag("brief");
}

void
XMLWriter::
writeText(
    Javadoc::Text const& node)
{
    indent() <<
        "<text>" <<
        escape(node.string) <<
        "</text>\n";
}

void
XMLWriter::
writeStyledText(
    Javadoc::StyledText const& node)
{
    writeTag(toString(node.style), node.string);
}

void
XMLWriter::
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
        indent() << "<code/>\n";
        return;
    }

    openTag("code");
    writeNodes(code.children);
    closeTag("code");
}

void
XMLWriter::
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
XMLWriter::
writeParam(
    Javadoc::Param const& param)
{
    openTag("param", {
        { "name", param.name, ! param.name.empty() }});
    writeNodes(param.children);
    closeTag("param");
}

void
XMLWriter::
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
XMLWriter::
openTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    indent() << '<' << tag << attrs << ">\n";
    adjustNesting(1);
}

void
XMLWriter::
closeTag(
    llvm::StringRef tag)
{
    adjustNesting(-1);
    indent() << "</" << tag << ">\n";
}

void
XMLWriter::
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

llvm::raw_ostream&
XMLWriter::
indent()
{
    return os_ << indentString_;
}

void
XMLWriter::
adjustNesting(int levels)
{
    if(levels > 0)
    {
        indentString_.append(levels * 2, ' ');
    }
    else
    {
        auto const n = levels * -2;
        Assert(n <= indentString_.size());
        indentString_.resize(indentString_.size() - n);
    }
}

//------------------------------------------------

std::string
XMLWriter::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
XMLWriter::
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
XMLWriter::
toString(
    Javadoc::Style style) noexcept
{
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

} // xml
} // mrdox
} // clang
