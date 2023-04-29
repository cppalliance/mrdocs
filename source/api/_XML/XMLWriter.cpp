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
#include "CXXTags.hpp"
#include "Support/Radix.hpp"
#include "Support/Operator.hpp"

namespace clang {
namespace mrdox {
namespace xml {

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
// XMLWriter
//
//------------------------------------------------

XMLWriter::
XMLWriter(
    llvm::raw_ostream& os,
    llvm::raw_fd_ostream* fd_os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : tags_(os)
    , os_(os)
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

    //writeAllSymbols();

    if(! corpus_.visit(globalNamespaceID, *this))
    {
        if(fd_os_ && fd_os_->error())
            return makeError("raw_fd_ostream returned ", fd_os_->error());
        return makeError("visit failed");
    }

    os_ << "</mrdox>\n";

    return llvm::Error::success();
}

//------------------------------------------------

void
XMLWriter::
writeAllSymbols()
{
    std::string temp;
    temp.reserve(256);
    tags_.open("symbols");
    for(auto I : corpus_.allSymbols())
    {
        llvm::StringRef tag;
        switch(I->IT)
        {
        case InfoType::IT_namespace:
            tag = "namespace";
            break;
        case InfoType::IT_record:
            switch(static_cast<RecordInfo const*>(I)->TagType)
            {
            case TagTypeKind::TTK_Class:  tag = "class"; break;
            case TagTypeKind::TTK_Struct: tag = "struct"; break;
            case TagTypeKind::TTK_Union:  tag = "struct"; break;
            default:
                Assert(false);
            }
            break;
        case InfoType::IT_function:
            tag = "function";
            break;
        case InfoType::IT_typedef:
            if(static_cast<TypedefInfo const*>(I)->IsUsing)
                tag = "using";
            else
                tag = "typedef";
            break;
        case InfoType::IT_enum:
            tag = "enum";
            break;
        case InfoType::IT_variable: 
            tag = "variable";
            break;
        default:
            Assert(false);
        }
        tags_.write("symbol", {}, {
            { "name", I->getFullyQualifiedName(temp) },
            { "tag", tag },
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
    if(fd_os_ && fd_os_->error())
        return false;

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
    if(fd_os_ && fd_os_->error())
        return false;

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
        tags_.write("friend", "", { { id } });

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
    if(fd_os_ && fd_os_->error())
        return false;

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
    if(fd_os_ && fd_os_->error())
        return false;

    llvm::StringRef tag;
    if(I.IsUsing)
        tag = "using";
    else
        tag = "typedef";
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
    if(fd_os_ && fd_os_->error())
        return false;

    tags_.open("enum", {
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

    tags_.close("enum");

    return true;
}

bool
XMLWriter::
visit(
    VariableInfo const& I)
{
    if(fd_os_ && fd_os_->error())
        return false;

    tags_.open("variable", {
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

    tags_.close("variable");

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
    tags_.write("tparam", {}, {
        { "decl", I.Contents }
        });
}

void
XMLWriter::
writeMemberType(
    MemberTypeInfo const& I)
{
    tags_.write("data", {}, {
        { "name", I.Name },
        { "type", I.Type.Name },
        { "value", I.DefaultValue, ! I.DefaultValue.empty() },
        { I.Access },
        { I.Type.id } });
}

void
XMLWriter::
writeStorageClass(
    jit_indenter& os, StorageClass SC)
{
    switch(SC)
    {
    case StorageClass::SC_None:
        break;
    case StorageClass::SC_Extern:
        os << "<extern/>";
        break;
    case StorageClass::SC_Static:
        os << "<static/>";
        break;
    case StorageClass::SC_PrivateExtern:
        os << "<private-extern/>";
        break;
    case StorageClass::SC_Auto:
        os << "<sc-auto/>";
        break;
    case StorageClass::SC_Register:
        os << "<sc-register/>";
        break;
    default:
        llvm_unreachable("unknown StorageClass");
        break;
    }
}

//------------------------------------------------

void
XMLWriter::
writeJavadoc(
    llvm::Optional<Javadoc> const& javadoc)
{
    if(! javadoc.has_value())
        return;
    tags_.open("doc");
    if(auto brief = javadoc->getBrief())
        writeBrief(*brief);
    writeNodes(javadoc->getBlocks());
    writeNodes(javadoc->getParams());
    writeNodes(javadoc->getTParams());
    tags_.close("doc");
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
