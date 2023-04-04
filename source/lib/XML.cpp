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
#include "CorpusVisitor.hpp"
#include "Representation.h"
#include "base64.hpp"
#include <mrdox/Config.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>

//------------------------------------------------
/*
    DTD

    Tags
        ns          namespace
        udt         class, struct, union
        fn
        en
        ty

    Attributes
        n           name
        r           return type
        a           Access
*/
//------------------------------------------------

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------

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
        escape const& t);

private:
    llvm::StringRef s_;
};

llvm::raw_ostream&
operator<<(
    llvm::raw_ostream& os,
    escape const& t)
{
    std::size_t pos = 0;
    auto const size = t.s_.size();
    while(pos < size)
    {
    unescaped:
        auto const found = t.s_.find_first_of("<>&'\"", pos);
        if(found == llvm::StringRef::npos)
        {
            os.write(t.s_.data() + pos, t.s_.size() - pos);
            break;
        }
        os.write(t.s_.data() + pos, found - pos);
        pos = found;
        while(pos < size)
        {
            auto const c = t.s_[pos];
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
    return os;
}

//------------------------------------------------

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class XMLGenerator
    : public clang::mrdox::Generator
{

public:
    static char const* Format;

    XMLGenerator()
        : cfg_([]() -> Config&
        {
            static Config c;
            return c;
        }())
    {
    }

    explicit
    XMLGenerator(
        Config const& cfg) noexcept
        : cfg_(cfg)
    {
    }

    bool
    render(
        std::string& xml,
        Corpus const& corpus,
        Config const& cfg);

    llvm::Error
    generateDocs(
        llvm::StringRef RootDir,
        Corpus const& corpus,
        Config const& cfg) override;

    llvm::Error
    generateDocForInfo(
        clang::mrdox::Info* I,
        llvm::raw_ostream& os,
        clang::mrdox::Config const& cfg) override;

private:
    using Attrs =
        std::initializer_list<
            std::pair<
                llvm::StringRef,
                llvm::StringRef>>;

    //--------------------------------------------

    void writeNamespaces(std::vector<Reference> const& v);
    void writeNamespace(NamespaceInfo const& I);
    void writeRecords(std::vector<Reference> const& v);
    void writeRecord(RecordInfo const& I);
    void writeFunctionList(FunctionList const& fnList);
    void writeFunctionOverloads(FunctionOverloads const& fns);
    void writeFunction(FunctionInfo const& I);
    void writeEnums(std::vector<EnumInfo> const& v);
    void writeEnum(EnumInfo const& I);
    void writeTypedefs(std::vector<TypedefInfo> const& v);
    void writeTypedef(TypedefInfo const& I);

    //--------------------------------------------

    void writeInfoPart(Info const& I);
    void writeNamespaceList(llvm::SmallVector<Reference, 4> const& v);
    void writeRef(Reference const& ref);
    void writeLoc(llvm::ArrayRef<Location> const& loc);
    void writeLoc(std::optional<Location> const& loc);

    //--------------------------------------------

    void openTag(llvm::StringRef);
    void openTag(llvm::StringRef, Attrs);
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef);
    void writeTag(llvm::StringRef, Attrs);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value, Attrs);

    //--------------------------------------------

    std::string toString(SymbolID const& id);
    llvm::StringRef toString(AccessSpecifier access);

    //--------------------------------------------

    NamespaceInfo const*
    findGlobalNamespace();

    //--------------------------------------------

    Config const& cfg_;
    std::string level_;
    llvm::raw_ostream* os_ = nullptr;
    InfoMap const* infos_ = nullptr;
};

//------------------------------------------------

bool
XMLGenerator::
render(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    xml.clear();
    infos_ = &corpus.USRToInfo;
    auto const ns = findGlobalNamespace();
    assert(ns != nullptr);
    llvm::raw_string_ostream os(xml);
    os_ = &os;
    level_ = {};
#if 0
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
#endif
    writeNamespace(*ns);
#if 0
    *os_ <<
        "</mrdox>\n";
#endif
    return true;
}

llvm::Error
XMLGenerator::
generateDocs(
    llvm::StringRef RootDir,
    Corpus const& corpus,
    Config const& cfg)
{
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
    infos_ = &corpus.USRToInfo;
    auto const ns = findGlobalNamespace();
    if(! ns)
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "not found: (global namespace)");
    std::error_code ec;
    llvm::raw_fd_ostream os(filename, ec);
    if(ec)
        return llvm::createStringError(
            ec,
            "output file could not be opened");
    level_ = {};
    os_ = &os;
#if 0
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n" <<
        "<mrdox>\n";
#endif
    writeNamespace(*ns);
#if 0
    *os_ <<
        "</mrdox>\n";
#endif
    // VFALCO the error handling needs to be
    //        propagated through all write functions
    if(os.error())
        return llvm::createStringError(
            ec,
            "output stream failure");
    return llvm::Error::success();
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

//------------------------------------------------

void
XMLGenerator::
writeNamespaces(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
    {
        assert(ref.RefType == InfoType::IT_namespace);
        auto it = infos_->find(
            llvm::toHex(llvm::toStringRef(ref.USR)));
        assert(it != infos_->end());
        writeNamespace(
            *static_cast<NamespaceInfo const*>(
                it->second.get()));
    }
}

void
XMLGenerator::
writeNamespace(
    NamespaceInfo const& I)
{
    openTag("namespace", {
        { "name", I.Name },
        { "USR", toBase64(I.USR) } });
    writeInfoPart(I);
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    writeFunctionList(I.Children.functions);
    writeEnums(I.Children.Enums);
    writeTypedefs(I.Children.Typedefs);
    closeTag("namespace");
}

void
XMLGenerator::
writeRecords(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
    {
        assert(ref.RefType == InfoType::IT_record);
        auto it = infos_->find(
            llvm::toHex(llvm::toStringRef(ref.USR)));
        assert(it != infos_->end());
        writeRecord(
            *static_cast<RecordInfo const*>(
                it->second.get()));
    }
}

void
XMLGenerator::
writeRecord(
    RecordInfo const& I)
{
    openTag( "compound", {
        { "name", I.Name },
        { "USR", toBase64(I.USR) },
        { "tag", getTagType(I.TagType) },
        });
    writeInfoPart(I);
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    writeFunctionList(I.Children.functions);
    writeEnums(I.Children.Enums);
    writeTypedefs(I.Children.Typedefs);
    closeTag("compound");
}

void
XMLGenerator::
writeFunctionList(
    FunctionList const& fnList)
{
    for(auto const& fns : fnList)
        writeFunctionOverloads(fns);
}

void
XMLGenerator::
writeFunctionOverloads(
    FunctionOverloads const& fns)
{
    for(auto const& fn : fns)
        writeFunction(fn);
}

void
XMLGenerator::
writeFunction(
    FunctionInfo const& I)
{
    openTag("func", {
        { "name", I.Name },
        { "USR", toBase64(I.USR) },
        { "access", toString(I.Access) }
        });
    writeInfoPart(I);
    writeTagLine("return",
        I.ReturnType.Type.Name, {
            { "usr", toString(I.ReturnType.Type.USR) }
        });
    writeRef(I.ReturnType.Type);
    if(I.Template)
    {
        for(TemplateParamInfo const& tp : I.Template->Params)
            writeTag(
                "tp", {
                { "n", tp.Contents }
                });
    }
    for(FieldTypeInfo const& p : I.Params)
        writeTag("p", {
            { "n", p.Name },
            { "t", p.Type.Name }
            });
    writeLoc(I.Loc);
    closeTag("func");
}

void
XMLGenerator::
writeEnums(
    std::vector<EnumInfo> const& v)
{
    for(auto const& I : v)
        writeEnum(I);
}

void
XMLGenerator::
writeEnum(
    EnumInfo const& I)
{
    openTag("enum", {
        { "name", I.Name },
        { "USR", toBase64(I.USR) },
        });
    writeInfoPart(I);
    for(auto const& v : I.Members)
    {
        writeTag("value", {
            { "n", v.Name },
            { "v", v.Value },
            });
    }
    writeLoc(I.Loc);
    closeTag("enum");
}

void
XMLGenerator::
writeTypedefs(
    std::vector<TypedefInfo> const& v)
{
    for(auto const& I : v)
        writeTypedef(I);
}

void
XMLGenerator::
writeTypedef(
    TypedefInfo const& I)
{
    openTag("typedef", {
        { "name", I.Name },
        { "USR", toBase64(I.USR) }
        });
    writeInfoPart(I);
    writeTagLine("qualname", I.Underlying.Type.QualName);
    writeLoc(I.DefLoc);
    closeTag("typedef");
}

//------------------------------------------------

void
XMLGenerator::
writeInfoPart(
    Info const& I)
{
    writeTagLine("type", std::to_string(static_cast<int>(I.IT)));
    writeNamespaceList(I.Namespace);
}

void
XMLGenerator::
writeNamespaceList(
    llvm::SmallVector<Reference, 4> const& v)
{
    for(auto const& ns : v)
        writeTagLine("ns", ns.QualName);
}

void
XMLGenerator::
writeRef(
    Reference const& ref)
{
    openTag("ref", {
        { "usr", toString(ref.USR) }
        });
    //writeTagLine("relpath", ref.getRelativeFilePath());
    writeTagLine("basename",
        ref.getFileBaseName());
    writeTagLine("name", ref.Name);
    writeTagLine("qual", ref.QualName);
    writeTagLine("type", std::to_string(static_cast<int>(ref.RefType)));
    writeTagLine("path", ref.Path);
    closeTag("ref");
}

void
XMLGenerator::
writeLoc(
    llvm::ArrayRef<Location> const& loc)
{
    if(loc.size() > 0)
        *os_ << level_ <<
            "<file>" << escape(loc[0].Filename) <<
            "</file><line>" << std::to_string(loc[0].LineNumber) <<
            "</line>\n";
}

void
XMLGenerator::
writeLoc(
    std::optional<Location> const& opt)
{
    if(! opt)
        return;
    Location const& loc(*opt);
    writeLoc(llvm::ArrayRef<Location>(&loc, &loc+1));
}

//------------------------------------------------

void
XMLGenerator::
openTag(
    llvm::StringRef tag)
{
    *os_ << level_ << '<' << tag << ">\n";
    level_.push_back(' ');
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
    level_.push_back(' ');
}

void
XMLGenerator::
closeTag(
    llvm::StringRef tag)
{
    level_.pop_back();
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
        return "0";
    case AccessSpecifier::AS_protected:
        return "1";
    case AccessSpecifier::AS_private:
        return "2";
    case AccessSpecifier::AS_none:
    default:
        return "3";
    }
}

//------------------------------------------------

NamespaceInfo const*
XMLGenerator::
findGlobalNamespace()
{
    for(auto const& g : *infos_)
    {
        auto const& inf = *g.getValue().get();
        if( inf.Name == "" &&
            inf.IT == InfoType::IT_namespace)
            return static_cast<
                NamespaceInfo const*>(&inf);
    }

    return nullptr;
}

//------------------------------------------------

char const*
XMLGenerator::
Format = "xml";

} // (anon)

//------------------------------------------------

void
renderToXMLString(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg)
{
    XMLGenerator G(cfg);
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
