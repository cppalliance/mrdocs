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

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------
//
// XMLGenerator
//
//------------------------------------------------

bool
XMLGenerator::
buildOne(
    llvm::StringRef fileName,
    Corpus const& corpus,
    Config const& config,
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
    if(ec)
    {
        R.failed("llvm::raw_fd_ostream", ec);
        return false;
    }

    Writer w(corpus, config, R);
    return w.build(os);
}

bool
XMLGenerator::
buildString(
    std::string& dest,
    Corpus const& corpus,
    Config const& config,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);
    Writer w(corpus, config, R);
    return w.build(os);
}

//------------------------------------------------
//
// Writer
//
//------------------------------------------------

bool
Writer::
build(
    llvm::raw_ostream& os)
{
    auto const ns = findGlobalNamespace();
    if(! ns)
    {
#if 0
        auto err = llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "not found: (global namespace)");
        R_.failed("findGlobalNamespace", err);
        return false;
#else
        // VFALCO This needs cleaning up
        return true;
#endif
    }
    os_ = &os;
    level_ = {};
#if 0
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
#endif
    writeAllSymbols();
    write(*ns);
#if 0
    *os_ <<
        "</mrdox>\n";
#endif
    return true;
}

//------------------------------------------------

void
Writer::
writeAllSymbols()
{
    openTag("all");
    std::string temp;
    for(auto const& id : corpus_.allSymbols)
    {
        auto const& I = corpus_.at(id);
        writeTag("symbol", {
            { "name", I.getFullyQualifiedName(temp) },
            { I.USR }
            });
    }
    closeTag("all");
}

//------------------------------------------------

void
Writer::
writeNamespaces(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
        write(corpus_.get<NamespaceInfo>(ref.USR));
}

void
Writer::
writeRecords(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
        write(corpus_.get<RecordInfo>(ref.USR));
}

void
Writer::
writeFunctions(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
        write(corpus_.get<FunctionInfo>(ref.USR));
}

void
Writer::
write(
    std::vector<EnumInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

void
Writer::
write(
    std::vector<TypedefInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

//------------------------------------------------

void
Writer::
write(
    NamespaceInfo const& I)
{
    assertExists(I);

    openTag("namespace", {
        { "name", I.Name },
        { I.USR }
        });
    writeInfo(I);
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    writeFunctions(I.Children.Functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("namespace");
}

void
Writer::
write(
    RecordInfo const& I)
{
    assertExists(I);

    llvm::StringRef tag;
    switch(I.TagType)
    {
    case TagTypeKind::TTK_Struct: tag = "struct"; break;
    case TagTypeKind::TTK_Class: tag = "class"; break;
    case TagTypeKind::TTK_Union: tag = "union"; break;
    default:
        llvm_unreachable("unexpected TagType");
    }
    openTag(tag, {
        { "name", I.Name },
        { I.USR }
        });
    writeSymbolInfo(I);
    writeRecords(I.Children.Records);
    writeFunctions(I.Children.Functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag(tag);
}

void
Writer::
write(
    FunctionInfo const& I)
{
    //assertExists(I);

    openTag("function", {
        { "name", I.Name },
        { "access", toString(I.Access) },
        { I.USR }
        });
    writeSymbolInfo(I);
    writeTag("return", {
        { "name", I.ReturnType.Type.Name },
        { I.ReturnType.Type.USR }
        });

    write(I.Params);

    write(I.ReturnType.Type);
    if(I.Template)
    {
        for(TemplateParamInfo const& tp : I.Template->Params)
            writeTag(
                "tp", {
                { "n", tp.Contents }
                });
    }
    write(I.Loc);

    closeTag("function");
}

void
Writer::
write(
    EnumInfo const& I)
{
    //assertExists(I);

    openTag("enum", {
        { "name", I.Name },
        { I.USR }
        });
    writeInfo(I);
    for(auto const& v : I.Members)
    {
        writeTag("element", {
            { "name", v.Name },
            { "value", v.Value },
            });
    }
    closeTag("enum");
}

void
Writer::
write(
    TypedefInfo const& I)
{
    //assertExists(I);

    openTag("typedef", {
        { "name", I.Name },
        { I.USR }
        });
    writeSymbolInfo(I);
    if(I.Underlying.Type.USR != EmptySID)
        writeTagLine("qualusr", toBase64(I.Underlying.Type.USR));
    closeTag("typedef");
}

//------------------------------------------------

void
Writer::
write(
    llvm::ArrayRef<FieldTypeInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

void
Writer::
write(FieldTypeInfo const& I)
{
    writeTag("param", {
        { "name", I.Name },
        { "default", I.DefaultValue, ! I.DefaultValue.empty() },
        { "type", I.Type.Name },
        { "reftype", toString(I.Type.RefType) },
        { I.Type.USR }
        });
}

void
Writer::
writeNamespaceRefs(
    llvm::SmallVector<Reference, 4> const& v)
{
    for(auto const& ns : v)
        writeTagLine("ns", ns.Name);
}

void
Writer::
write(
    Reference const& I)
{
    //openTag("ref", {
        //{ "usr", toString(ref.USR) }
        //});
    //writeTagLine("relpath", ref.getRelativeFilePath());
    //writeTagLine("basename",I.getFileBaseName());
    writeTagLine("name", I.Name);
    writeTagLine("tag", std::to_string(static_cast<int>(I.RefType)));
    writeTagLine("path", I.Path);
    //closeTag("ref");
}

//------------------------------------------------

void
Writer::
writeInfo(
    Info const& I)
{
#if 0
    writeTagLine("extract-name", I.extractName());
    auto relPath = I.getRelativeFilePath(config_.SourceRoot);
    if(! relPath.empty())
        writeTagLine("rel-path", relPath);
    writeTagLine("base-name", I.getFileBaseName());
#endif
}

void
Writer::
writeSymbolInfo(
    SymbolInfo const& I)
{
    writeInfo(I);
    if(I.DefLoc)
        write(*I.DefLoc);
    write(I.Loc);
}

void
Writer::
write(
    llvm::ArrayRef<Location> const& locs)
{
    for(auto const& loc : locs)
        write(loc);
}

void
Writer::
write(
    Location const& loc)
{
    *os_ << level_ <<
        "<file>" << escape(loc.Filename) <<
        "</file><line>" << std::to_string(loc.LineNumber) <<
        "</line>\n";
}

//------------------------------------------------

void
Writer::
openTag(
    llvm::StringRef tag)
{
    *os_ << level_ << '<' << tag << ">\n";
    indent();
}

void
Writer::
openTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    *os_ << level_ << '<' << tag;
    writeAttrs(attrs);
    *os_ << ">\n";
    indent();
}

void
Writer::
closeTag(
    llvm::StringRef tag)
{
    outdent();
    *os_ << level_ << "</" << tag << ">\n";
}

void
Writer::
writeTag(
    llvm::StringRef tag)
{
    *os_ << level_ << "<" << tag << "/>\n";
}

void
Writer::
writeTag(
    llvm::StringRef tag,
    Attrs attrs)
{
    *os_ << level_ << "<" << tag;
    writeAttrs(attrs);
    *os_ << "/>\n";
}

void
Writer::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value)
{
    *os_ << level_ <<
        "<" << tag << ">" <<
        escape(value) <<
        "</" << tag << ">" <<
        "\n";
}

void
Writer::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value,
    Attrs attrs)
{
    *os_ << level_ << "<" << tag;
    writeAttrs(attrs);
    *os_ << ">" << escape(value) << "</" << tag << ">" << "\n";
}

void
Writer::
writeAttrs(
    Attrs attrs)
{
    for(auto const& attr : attrs)
        if(attr.pred)
            *os_ <<
                ' ' << attr.name << '=' <<
                "\"" << escape(attr.value) << "\"";
}

void
Writer::
indent()
{
    level_.append("    ");
}

void
Writer::
outdent()
{
    level_.resize(level_.size() - 4);
}

//------------------------------------------------

std::string
Writer::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
Writer::
toString(
    AccessSpecifier access)
{
    switch(access)
    {
    case AccessSpecifier::AS_public:
        return "public";
    case AccessSpecifier::AS_protected:
        return "protected";
    case AccessSpecifier::AS_private:
        return "private";
    case AccessSpecifier::AS_none:
    default:
        return "none";
    }
}

//------------------------------------------------

NamespaceInfo const*
Writer::
findGlobalNamespace()
{
    auto p = corpus_.find(EmptySID);
    if(p != nullptr)
    {
        assert(p->Name.empty());
        assert(p->IT == InfoType::IT_namespace);
        return static_cast<NamespaceInfo const*>(p);
    }
    return nullptr;
}

#ifndef NDEBUG
void
Writer::
assertExists(
    Info const& I)
{
    assert(corpus_.exists(I.USR));
}
#endif

llvm::StringRef
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

//------------------------------------------------

llvm::Error
Writer::
generateDocForInfo(
    clang::mrdox::Info* I,
    llvm::raw_ostream& os,
    clang::mrdox::Config const& config)
{
    return llvm::Error::success();
}

} // (anon)

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<XMLGenerator>();
}

} // mrdox
} // clang
