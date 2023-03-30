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

#include <mrdox/mrdox.hpp>

#include "Generators.h"
#include "Representation.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

//------------------------------------------------

namespace clang {
namespace doc {

//------------------------------------------------

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class XMLGenerator
    : public clang::doc::Generator
{

public:
    using InfoMap = llvm::StringMap<
        std::unique_ptr<doc::Info>>;

    static char const* Format;

    llvm::Error
    generateDocs(
        llvm::StringRef RootDir,
        InfoMap Infos,
        ClangDocContext const& CDCtx) override;

    llvm::Error
    createResources(
        ClangDocContext& CDCtx) override;

    llvm::Error
    generateDocForInfo(
        clang::doc::Info* I,
        llvm::raw_ostream& os,
        clang::doc::ClangDocContext const& CDCtx) override;

private:
    using Attrs =
        std::initializer_list<
            std::pair<
                llvm::StringRef,
                llvm::StringRef>>;

    NamespaceInfo const*
    findGlobalNamespace();

    void openTag(llvm::StringRef);
    void openTag(llvm::StringRef, Attrs);
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef);
    void writeTag(llvm::StringRef, Attrs);

    void writeHeader();
    void write(NamespaceInfo const& I);
    void write(RecordInfo const& I);
    void write(FunctionInfo const& I);
    void write(EnumInfo const& I);
    void write(TypedefInfo const& I);
    void write(mrdox::FunctionOverloads const& fns);
    void write(mrdox::FunctionList const& fnList);
    void write(std::vector<EnumInfo> const& v);
    void write(std::vector<TypedefInfo> const& v);

    void writeNamespaces(std::vector<Reference> const& v);
    void writeRecords(std::vector<Reference> const& v);
    
    std::string level_;
    llvm::raw_fd_ostream* os_ = nullptr;
    InfoMap const* infos_ = nullptr;
};

//------------------------------------------------

llvm::Error
XMLGenerator::
generateDocs(
    llvm::StringRef RootDir,
    InfoMap Infos,
    ClangDocContext const& CDCtx)
{
    llvm::SmallString<256> filename(CDCtx.OutDirectory);
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
    infos_ = &Infos;
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
    os_ = &os;
    writeHeader();
    level_ = {};
    write(*ns);
    if(os_->error())
        return llvm::createStringError(
            ec,
            "output stream failure");
    return llvm::Error::success();
}

llvm::Error
XMLGenerator::
createResources(
    ClangDocContext& CDCtx)
{
    return llvm::Error::success();
}

llvm::Error
XMLGenerator::
generateDocForInfo(
    clang::doc::Info* I,
    llvm::raw_ostream& os,
    clang::doc::ClangDocContext const& CDCtx)
{
    return llvm::Error::success();
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

void
XMLGenerator::
openTag(
    llvm::StringRef tag)
{
    *os_ << level_ << '<' << tag << ">\n";
    level_.push_back(' ');
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
            "\"" << attr.second << "\"";
    *os_ << ">\n";
    level_.push_back(' ');
    level_.push_back(' ');
}

void
XMLGenerator::
closeTag(
    llvm::StringRef tag)
{
    level_.pop_back();
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
            "\"" << attr.second << "\"";
    *os_ << "/>\n";
}

//------------------------------------------------

void
XMLGenerator::
writeHeader()
{
    *os_ <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" <<
        "<!DOCTYPE mrdox SYSTEM \"mrdox.dtd\">\n";
}

void
XMLGenerator::
write(
    NamespaceInfo const& I)
{
    openTag("namespace", {
        { "name", I.Name }
        });
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    write(I.Children.functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("namespace");
}

//------------------------------------------------

void
XMLGenerator::
write(
    RecordInfo const& I)
{
    openTag("compound", {
        { "name", I.Name },
        { "type", getTagType(I.TagType) }
        });
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    write(I.Children.functions);
    write(I.Children.Enums);
    write(I.Children.Typedefs);
    closeTag("compound");
}
//------------------------------------------------

void
XMLGenerator::
write(
    FunctionInfo const& I)
{
    openTag("function", {
        { "name", I.Name },
        { "returns", I.ReturnType.Type.Name }
        });
    for(FieldTypeInfo const& p : I.Params)
        writeTag("param", {
            { "name", p.Name },
            { "type", p.Type.Name }
            });

    closeTag("function");
}

//------------------------------------------------

void
XMLGenerator::
write(
    EnumInfo const& I)
{
    openTag("enum", {
        { "name", I.Name },
        });
    for(auto const& v : I.Members)
    {
        writeTag("value", {
            { "name", v.Name },
            { "value", v.Value },
            });
    }

    closeTag("enum");
}

//------------------------------------------------

void
XMLGenerator::
write(
    TypedefInfo const& I)
{
    writeTag("typedef", {
        { "name", I.Name }
        });
}

//------------------------------------------------

void
XMLGenerator::
write(
    mrdox::FunctionOverloads const& fns)
{
    for(auto const& fn : fns)
        write(fn);
}

void
XMLGenerator::
write(
    mrdox::FunctionList const& fnList)
{
    for(auto const& fns : fnList)
        write(fns);
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
writeNamespaces(
    std::vector<Reference> const& v)
{
    for(auto const& ref : v)
    {
        assert(ref.RefType == InfoType::IT_namespace);
        auto it = infos_->find(
            llvm::toHex(llvm::toStringRef(ref.USR)));
        assert(it != infos_->end());
        write(*static_cast<NamespaceInfo const*>(
            it->second.get()));
    }
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
        write(*static_cast<RecordInfo const*>(
            it->second.get()));
    }
}

//------------------------------------------------

char const*
XMLGenerator::
Format = "xml";

} // doc
} // clang

//------------------------------------------------

namespace mrdox {

llvm::Expected<llvm::Twine>
renderXML(
    llvm::StringRef path)
{
    return llvm::Twine();
}

} // mrdox

//------------------------------------------------

using namespace llvm;

namespace clang {
namespace doc {

static
clang::doc::GeneratorRegistry::Add<
    clang::doc::XMLGenerator>
xmlGenerator(
    clang::doc::XMLGenerator::Format,
    "Generator for XML output.");

// This anchor is used to force the linker
// to link in the generated object file and
// thus register the generator.
volatile int XMLGeneratorAnchorSource = 0;

}
}

void
force_xml_generator_linkage()
{
    // VFALCO This whole business of disappearing
    //        TUs needs to be refactored.
    clang::doc::XMLGeneratorAnchorSource++;
}
