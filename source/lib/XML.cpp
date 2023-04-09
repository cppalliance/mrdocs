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

#include "Generators.h"
#include "Representation.h"
#include "Namespace.hpp"
#include "base64.hpp"
#include "escape.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Visitor.hpp>
#include <clang/Index/USRGeneration.h>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>

//------------------------------------------------

namespace clang {
namespace mrdox {

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class XMLGenerator
    : public clang::mrdox::Generator
{
    Corpus const* corpus_ = nullptr;
    Config const* cfg_ = nullptr;
    std::string level_;
    llvm::raw_ostream* os_ = nullptr;

    using Attrs =
        std::initializer_list<
            std::pair<
                llvm::StringRef,
                llvm::StringRef>>;

    void writeAllSymbols();

    //--------------------------------------------

    void writeNamespaces(std::vector<Reference> const& v);
    void writeRecords(std::vector<Reference> const& v);
    void write(std::vector<FunctionInfo> const& v);
    void write(std::vector<EnumInfo> const& v);
    void write(std::vector<TypedefInfo> const& v);

    //--------------------------------------------

    void write(NamespaceInfo const& I);
    void write(RecordInfo const& I);
    void write(FunctionInfo const& I);
    void write(EnumInfo const& I);
    void write(TypedefInfo const& I);

    //--------------------------------------------

    void write(llvm::ArrayRef<FieldTypeInfo> const& v);
    void write(FieldTypeInfo const& I);
    void writeNamespaceRefs(llvm::SmallVector<Reference, 4> const& v);
    void write(Reference const& ref);

    //--------------------------------------------

    void writeInfo(Info const& I);
    void writeSymbolInfo(SymbolInfo const& I);
    void write(llvm::ArrayRef<Location> const& locs);
    void write(Location const& loc);

    //--------------------------------------------

    void openTag(llvm::StringRef);
    void openTag(llvm::StringRef, Attrs);
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef);
    void writeTag(llvm::StringRef, Attrs);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value, Attrs);
    void indent();
    void outdent();

    //--------------------------------------------

    std::string toString(SymbolID const& id);
    llvm::StringRef toString(AccessSpecifier access);
    NamespaceInfo const* findGlobalNamespace();
#ifndef NDEBUG
    void assertExists(Info const& I);
#else
    void assertExists(Info const&) const noexcept
    {
    }
#endif

    //--------------------------------------------

    static llvm::StringRef toString(InfoType) noexcept;

public:
    static char const* Format;

    llvm::Error render(llvm::raw_ostream& os);

    bool
    render(
        std::string& xml,
        Corpus const& corpus,
        Config const& cfg);

    llvm::Error
    generateDocForInfo(
        clang::mrdox::Info* I,
        llvm::raw_ostream& os,
        clang::mrdox::Config const& cfg) override;

    llvm::Error
    generateDocs(
        llvm::StringRef RootDir,
        Corpus const& corpus,
        Config const& cfg) override;
};

//------------------------------------------------

void
XMLGenerator::
writeAllSymbols()
{
    openTag("all");
    std::string temp;
    for(auto const& id : corpus_->allSymbols)
    {
        auto const& I = corpus_->at(id);
        writeTag("symbol", {
            { "name", I.getFullyQualifiedName(temp) },
            { "usr", toBase64(I.USR) }
            });
    }
    closeTag("all");
}

//------------------------------------------------

void
XMLGenerator::
writeNamespaces(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
        write(corpus_->get<NamespaceInfo>(ref.USR));
}

void
XMLGenerator::
writeRecords(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
        write(corpus_->get<RecordInfo>(ref.USR));
}

void
XMLGenerator::
write(
    std::vector<FunctionInfo> const& v)
{
    for(auto const& fn : v)
        write(fn);
}

void
XMLGenerator::
write(
    std::vector<EnumInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

void
XMLGenerator::
write(
    std::vector<TypedefInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

//------------------------------------------------

void
XMLGenerator::
write(
    NamespaceInfo const& I)
{
    assertExists(I);

    openTag("namespace", {
        { "name", I.Name },
        { "usr", toBase64(I.USR) }
        });
    writeInfo(I);
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    write(I.Children.Functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("namespace");
}

void
XMLGenerator::
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
        { "usr", toBase64(I.USR) }
        });
    writeSymbolInfo(I);
    writeRecords(I.Children.Records);
    write(I.Children.Functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag(tag);
}

void
XMLGenerator::
write(
    FunctionInfo const& I)
{
    //assertExists(I);

    openTag("function", {
        { "name", I.Name },
        { "access", toString(I.Access) },
        { "usr", toBase64(I.USR) }
        });
    writeSymbolInfo(I);
    writeTag("return", {
        { "name", I.ReturnType.Type.Name },
        { "usr", toString(I.ReturnType.Type.USR) }
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
XMLGenerator::
write(
    EnumInfo const& I)
{
    //assertExists(I);

    openTag("enum", {
        { "name", I.Name },
        { "usr", toBase64(I.USR) },
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
XMLGenerator::
write(
    TypedefInfo const& I)
{
    //assertExists(I);

    openTag("typedef", {
        { "name", I.Name },
        { "usr", toString(I.USR) }
        });
    writeSymbolInfo(I);
    writeTagLine("qualname", I.Underlying.Type.QualName);
    writeTagLine("qualusr", toBase64(I.Underlying.Type.USR));
    closeTag("typedef");
}

//------------------------------------------------

void
XMLGenerator::
write(
    llvm::ArrayRef<FieldTypeInfo> const& v)
{
    for(auto const& I : v)
        write(I);
}

void
XMLGenerator::
write(FieldTypeInfo const& I)
{
    writeTag("param", {
        { "name", I.Name },
        { "default", I.DefaultValue },
        { "type", I.Type.Name },
        { "qualname", I.Type.QualName },
        { "reftype", toString(I.Type.RefType) },
        { "usr", toString(I.Type.USR) }
        });
}

void
XMLGenerator::
writeNamespaceRefs(
    llvm::SmallVector<Reference, 4> const& v)
{
    for(auto const& ns : v)
        writeTagLine("ns", ns.QualName);
}

void
XMLGenerator::
write(
    Reference const& I)
{
    //openTag("ref", {
        //{ "usr", toString(ref.USR) }
        //});
    //writeTagLine("relpath", ref.getRelativeFilePath());
    writeTagLine("basename",
        I.getFileBaseName());
    writeTagLine("name", I.Name);
    writeTagLine("qual", I.QualName);
    writeTagLine("tag", std::to_string(static_cast<int>(I.RefType)));
    writeTagLine("path", I.Path);
    //closeTag("ref");
}

//------------------------------------------------

void
XMLGenerator::
writeInfo(
    Info const& I)
{
// VFALCO for now...
return;
    writeTagLine("extract-name", I.extractName());
    writeTagLine("rel-path", I.getRelativeFilePath(cfg_->SourceRoot));
    writeTagLine("base-name", I.getFileBaseName());
}

void
XMLGenerator::
writeSymbolInfo(
    SymbolInfo const& I)
{
    writeInfo(I);
    if(I.DefLoc)
        write(*I.DefLoc);
    write(I.Loc);
}

void
XMLGenerator::
write(
    llvm::ArrayRef<Location> const& locs)
{
    for(auto const& loc : locs)
        write(loc);
}

void
XMLGenerator::
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
XMLGenerator::
openTag(
    llvm::StringRef tag)
{
    *os_ << level_ << '<' << tag << ">\n";
    indent();
}

void
XMLGenerator::
openTag(
    llvm::StringRef tag,
    Attrs init)
{
    *os_ << level_ << '<' << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << ">\n";
    indent();
}

void
XMLGenerator::
closeTag(
    llvm::StringRef tag)
{
    outdent();
    *os_ << level_ << "</" << tag << ">\n";
}

void
XMLGenerator::
writeTag(
    llvm::StringRef tag)
{
    *os_ << level_ << "<" << tag << "/>\n";
}

void
XMLGenerator::
writeTag(
    llvm::StringRef tag,
    Attrs init)
{
    *os_ << level_ << "<" << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << "/>\n";
}

void
XMLGenerator::
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
XMLGenerator::
writeTagLine(
    llvm::StringRef tag,
    llvm::StringRef value,
    Attrs init)
{
    *os_ << level_ <<
        "<" << tag;
    for(auto const& attr : init)
        *os_ <<
            ' ' << attr.first << '=' <<
            "\"" << escape(attr.second) << "\"";
    *os_ << ">" << escape(value) << "</" << tag << ">" << "\n";
}

void
XMLGenerator::
indent()
{
    level_.append("    ");
}

void
XMLGenerator::
outdent()
{
    level_.resize(level_.size() - 4);
}

//------------------------------------------------

std::string
XMLGenerator::
toString(
    SymbolID const& id)
{
    return toBase64(id);
}

llvm::StringRef
XMLGenerator::
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
XMLGenerator::
findGlobalNamespace()
{
    for(auto const& g : corpus_->InfoMap)
    {
        auto const& inf = *g.getValue().get();
        if( inf.Name == "" &&
            inf.IT == InfoType::IT_namespace)
            return static_cast<
                NamespaceInfo const*>(&inf);
    }

    return nullptr;
}

#ifndef NDEBUG
void
XMLGenerator::
assertExists(
    Info const& I)
{
    assert(corpus_->exists(I.USR));
}
#endif

llvm::StringRef
XMLGenerator::
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
XMLGenerator::
render(
    llvm::raw_ostream& os)
{
    auto const ns = findGlobalNamespace();
    if(! ns)
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "not found: (global namespace)");

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
    return llvm::Error::success();
}

bool
XMLGenerator::
render(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    xml.clear();
    corpus_ = &corpus;
    cfg_ = &cfg;
    llvm::raw_string_ostream os(xml);
    llvm::Error err = render(os);
    if(err)
        return false;
    return true;
}

llvm::Error
XMLGenerator::
generateDocForInfo(
    clang::mrdox::Info* I,
    llvm::raw_ostream& os,
    clang::mrdox::Config const& cfg)
{
    return llvm::Error::success();
}

llvm::Error
XMLGenerator::
generateDocs(
    llvm::StringRef RootDir,
    Corpus const& corpus,
    Config const& cfg)
{
    corpus_ = &corpus;
    cfg_ = &cfg;
    llvm::SmallString<256> filename(cfg.OutDirectory);
    if(! fs::is_directory(filename))
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "OutDirectory is not a directory");
    path::append(filename, "index.xml");
    if( fs::exists(filename) &&
        ! fs::is_regular_file(filename))
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "Output file is not regular");
    std::error_code ec;
    llvm::raw_fd_ostream os(filename, ec);
    if(ec)
        return llvm::createStringError(
            ec,
            "output file could not be opened");
    llvm::Error err = render(os);
    if(err)
        return err;
    // VFALCO the error handling needs to be
    //        propagated through all write functions
    if(os.error())
        return llvm::createStringError(
            ec,
            "output stream failure");
    return llvm::Error::success();
}

//------------------------------------------------

char const*
XMLGenerator::
Format = "xml";

//------------------------------------------------

void
renderToXMLString(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    XMLGenerator G;
    G.render(xml, corpus, cfg);
    //return llvm::Error::success();
}

//------------------------------------------------

static
GeneratorRegistry::
Add<XMLGenerator>
xmlGenerator(
    XMLGenerator::Format,
    "Generator for XML output.");

// This anchor is used to force the linker
// to link in the generated object file and
// thus register the generator.
volatile int XMLGeneratorAnchorSource = 0;

using namespace llvm;
} // mrdox
} // clang

void
force_xml_generator_linkage()
{
    // VFALCO This whole business of disappearing
    //        TUs needs to be refactored.
    clang::mrdox::XMLGeneratorAnchorSource++;
}
