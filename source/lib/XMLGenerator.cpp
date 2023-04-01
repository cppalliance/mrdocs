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
#include <mrdox/ClangDocContext.hpp>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

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

private:
    llvm::StringRef s_;
};

//------------------------------------------------

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class XMLGenerator
    : public clang::mrdox::Generator
{

public:
    using InfoMap = llvm::StringMap<
        std::unique_ptr<mrdox::Info>>;

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
        clang::mrdox::Info* I,
        llvm::raw_ostream& os,
        clang::mrdox::ClangDocContext const& CDCtx) override;

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

    void writeNamespace(NamespaceInfo const& I);
    void writeRecord(RecordInfo const& I);
    void writeFunction(FunctionInfo const& I);
    void writeEnum(EnumInfo const& I);
    void writeTypedef(TypedefInfo const& I);
    void writeOverloads(FunctionOverloads const& fns);
    void writeFunctions(FunctionList const& fnList);
    void writeEnums(std::vector<EnumInfo> const& v);
    void writeTypedefs(std::vector<TypedefInfo> const& v);

    void writeNamespaces(std::vector<Reference> const& v);
    void writeRecords(std::vector<Reference> const& v);
    
    std::string level_;
    llvm::raw_fd_ostream* os_ = nullptr;
    InfoMap const* infos_ = nullptr;
};

//------------------------------------------------


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
    clang::mrdox::Info* I,
    llvm::raw_ostream& os,
    clang::mrdox::ClangDocContext const& CDCtx)
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

//------------------------------------------------

void
XMLGenerator::
writeNamespace(
    NamespaceInfo const& I)
{
    openTag(
        "ns", {
        { "n", I.Name }
        });
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    writeFunctions(I.Children.functions);
    writeEnums(I.Children.Enums);
    writeTypedefs(I.Children.Typedefs);
    closeTag("ns");
}

//------------------------------------------------

void
XMLGenerator::
writeRecord(
    RecordInfo const& I)
{
    openTag(
        "udt", {
        { "n", I.Name },
        { "t", getTagType(I.TagType) }
        });
    writeNamespaces(I.Children.Namespaces);
    writeRecords(I.Children.Records);
    writeFunctions(I.Children.functions);
    writeEnums(I.Children.Enums);
    writeTypedefs(I.Children.Typedefs);
    closeTag("udt");
}
//------------------------------------------------

static
llvm::StringRef
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

void
XMLGenerator::
writeFunction(
    FunctionInfo const& I)
{
    openTag("fn", {
        { "n", I.Name },
        { "r", I.ReturnType.Type.Name },
        { "a", toString(I.Access) }
        });
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
    closeTag("fn");
}

//------------------------------------------------

void
XMLGenerator::
writeEnum(
    EnumInfo const& I)
{
    openTag("en", {
        { "n", I.Name },
        });
    for(auto const& v : I.Members)
    {
        writeTag("value", {
            { "n", v.Name },
            { "v", v.Value },
            });
    }
    closeTag("en");
}

//------------------------------------------------

void
XMLGenerator::
writeTypedef(
    TypedefInfo const& I)
{
    writeTag(
        "ty", {
        { "n", I.Name }
        });
}

//------------------------------------------------

void
XMLGenerator::
writeOverloads(
    FunctionOverloads const& fns)
{
    for(auto const& fn : fns)
        writeFunction(fn);
}

void
XMLGenerator::
writeFunctions(
    FunctionList const& fnList)
{
    for(auto const& fns : fnList)
        writeOverloads(fns);
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
writeTypedefs(
    std::vector<TypedefInfo> const& v)
{
    for(auto const& I : v)
        writeTypedef(I);
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

//------------------------------------------------

char const*
XMLGenerator::
Format = "xml";

} // (anon)

//------------------------------------------------

#if 0
llvm::Expected<llvm::Twine>
renderXML(
    llvm::StringRef fileName)
{
    if(! path::extension(fileName).equals_insensitive(".cpp"))
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "not a .cpp file");
    llvm::SmallString<256> dir(
        path::parent_path(fileName));

    return llvm::Twine();
}
#endif

//------------------------------------------------

static
GeneratorRegistry::Add<XMLGenerator>
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
