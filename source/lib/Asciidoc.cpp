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

#include "Asciidoc.hpp"
#include <mrdox/OverloadSet.hpp>
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// AsciidocGenerator
//
//------------------------------------------------

bool
AsciidocGenerator::
build(
    llvm::StringRef rootPath,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    namespace path = llvm::sys::path;

#if 0
    // Track which directories we already tried to create.
    llvm::StringSet<> CreatedDirs;

    // Collect all output by file name and create the necessary directories.
    llvm::StringMap<std::vector<mrdox::Info*>> FileToInfos;
    for (const auto& Group : corpus.InfoMap)
    {
        mrdox::Info* Info = Group.getValue().get();

        llvm::SmallString<128> Path;
        llvm::sys::path::native(RootDir, Path);
        llvm::sys::path::append(Path, Info->getRelativeFilePath(""));
        if (!CreatedDirs.contains(Path)) {
            if (std::error_code Err = llvm::sys::fs::create_directories(Path);
                Err != std::error_code()) {
                return llvm::createStringError(Err, "Failed to create directory '%s'.",
                    Path.c_str());
            }
            CreatedDirs.insert(Path);
        }

        llvm::sys::path::append(Path, Info->getFileBaseName() + ".adoc");
        FileToInfos[Path].push_back(Info);
    }

    for (const auto& Group : FileToInfos) {
        std::error_code FileErr;
        llvm::raw_fd_ostream InfoOS(Group.getKey(), FileErr,
            llvm::sys::fs::OF_None);
        if (FileErr) {
            return llvm::createStringError(FileErr, "Error opening file '%s'",
                Group.getKey().str().c_str());
        }

        for (const auto& Info : Group.getValue()) {
            if (llvm::Error Err = generateDocForInfo(Info, InfoOS, config)) {
                return Err;
            }
        }
    }
    return true;
#else
    llvm::SmallString<0> fileName(rootPath);
    path::append(fileName, "reference.adoc");
    return buildOne(fileName, corpus, config, R);
#endif
}

bool
AsciidocGenerator::
buildOne(
    llvm::StringRef fileName,
    Corpus& corpus,
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
    if(R.error(ec, "open the stream for '", fileName, "'"))
        return false;

    if(! corpus.canonicalize(R))
        return false;
    Writer w(os, corpus, config, R);
    w.beginFile();
    w.visitAllSymbols();
    w.endFile();
    return ! os.has_error();
}

bool
AsciidocGenerator::
buildString(
    std::string& dest,
    Corpus& corpus,
    Config const& config,
    Reporter& R) const
{
    dest.clear();
    llvm::raw_string_ostream os(dest);

    if(! corpus.canonicalize(R))
        return false;
    Writer w(os, corpus, config, R);
    w.beginFile();
    w.visitAllSymbols();
    w.endFile();
    return true;
}

//------------------------------------------------
//
// FlatWriter
//
//------------------------------------------------

struct AsciidocGenerator::Writer::
    FormalParam
{
    FieldTypeInfo const& I;
    Writer& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        FormalParam const& t)
    {
        t.w.writeFormalParam(t, os);
        return os;
    }
};

struct AsciidocGenerator::Writer::
    TypeName
{
    TypeInfo const& I;
    Writer& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        TypeName const& t)
    {
        t.w.writeTypeName(t, os);
        return os;
    }
};

//------------------------------------------------

AsciidocGenerator::
Writer::
Writer(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Config const& config,
    Reporter& R) noexcept
    : FlatWriter(os, corpus, config, R)
{
}

void
AsciidocGenerator::
Writer::
beginFile()
{
    openTitle("Reference");
    os_ <<
        ":role: mrdox\n"
        "\n";
}

void
AsciidocGenerator::
Writer::
endFile()
{
    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeFormalParam(
    FormalParam const& t,
    llvm::raw_ostream& os)
{
    auto const& I = t.I;
    os << I.Type.Name << ' ' << I.Name;
}

auto
AsciidocGenerator::
Writer::
formalParam(
    FieldTypeInfo const& t) ->
        FormalParam
{
    return FormalParam{ t, *this };
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeRecord(
    RecordInfo const& I)
{
    openSection(I.Name);
    if(! I.javadoc.brief.empty())
        os_ <<
            I.javadoc.brief << "\n";

    // Synopsis
    openSection("Synopsis");
    os_ <<
        "\n" <<
        "Located in <" << getLocation(I).Filename << ">\n" <<
        "[,cpp]\n"
        "----\n" <<
        toString(I.TagType) << " " << I.Name;
    if(! I.Bases.empty())
    {
        os_ << "\n    : ";
        writeBase(I.Bases[0]);
        for(std::size_t i = 1; i < I.Bases.size(); ++i)
        {
            os_ << "\n    , ";
            writeBase(I.Bases[i]);
        }
    }
    os_ <<
        ";\n"
        "----\n";
    closeSection();

    // Description
    if(! I.javadoc.desc.empty())
    {
        os_ << "\n";
        openSection("Description");
        os_ << I.javadoc.desc << "\n";
        closeSection();
    }

    // Data Members

    // Member Functions

    {
        // public
        auto list = makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_public;
            });
        if(! list.empty())
        {
            os_ << "\n";
            writeOverloadSet("Member Functions", list);
        }
    }

    {
        // protected
        auto list = makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_protected;
            });
        if(! list.empty())
        {
            os_ << "\n";
            writeOverloadSet("Protected Member Functions", list);
        }
    }

    if(! config_.PublicOnly)
    {
        // private
        auto list = makeOverloadSet(corpus_, I.Children,
            [](FunctionInfo const& I)
            {
                return I.Access == AccessSpecifier::AS_private;
            });
        if(! list.empty())
        {
            os_ << "\n";
            writeOverloadSet("Private Member Functions", list);
        }
    }

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeFunction(
    FunctionInfo const& I)
{
    openSection(I.Name);
    os_ << I.javadoc.brief << "\n\n";

    // Synopsis
    openSection("Synopsis");
    os_ <<
        "Located in <" << getLocation(I).Filename << ">\n" <<
        "[,cpp]\n"
        "----\n";

    // params
    if(! I.Params.empty())
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "(\n"
            "    " << formalParam(I.Params[0]);
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            os_ << ",\n"
                "    " << formalParam(I.Params[i]);
        }
        os_ << ");\n";
    }
    else
    {
        os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "();" << "\n";
    }

    os_ <<
        "----\n";
    closeSection();

    if(! I.javadoc.desc.empty())
    {
        os_ << "\n";
        openSection("Description");
        os_ << I.javadoc.desc << "\n";
        closeSection();
    }

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeEnum(
    EnumInfo const& I)
{
}

void
AsciidocGenerator::
Writer::
writeTypedef(
    TypedefInfo const& I)
{
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeBase(
    BaseRecordInfo const& I)
{
    os_ << clang::getAccessSpelling(I.Access) << " " << I.Name;
}

void
AsciidocGenerator::
Writer::
writeOverloadSet(
    llvm::StringRef sectionName,
    std::vector<OverloadSet> const& list)
{
    openSection(sectionName);
    os_ <<
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    for(auto const& J : list)
    {
        os_ <<
            "|`" << J.name << "`\n" <<
            "|";
        for(auto const& K : J.list)
            os_ << K->javadoc.brief << "\n";
    }   
    os_ <<
        "|===\n" <<
        "\n";
    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeTypeName(
    TypeName const& t,
    llvm::raw_ostream& os)
{
    if(t.I.Type.USR == EmptySID)
    {
        os_ << t.I.Type.Name;
        return;
    }
    auto p = corpus_.find<RecordInfo>(t.I.Type.USR);
    if(p != nullptr)
    {
        // VFALCO add namespace qualifiers if I is in
        //        a different namesapce
        os_ << p->Path << "::" << p->Name;
        return;
    }
    auto const& T = t.I.Type;
    os_ << T.Path << "::" << T.Name;
}

auto
AsciidocGenerator::
Writer::
typeName(
    TypeInfo const& t) ->
        TypeName
{
    return TypeName{ t, *this };
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
openTitle(
    llvm::StringRef name)
{
    assert(sect_.level == 0);
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        sect_.markup << ' ' << name << "\n";
}

void
AsciidocGenerator::
Writer::
openSection(
    llvm::StringRef name)
{
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        "\n" <<
        sect_.markup << ' ' << name << "\n";
}

void
AsciidocGenerator::
Writer::
closeSection()
{
    assert(sect_.level > 0);
    if(sect_.level <= 6)
        sect_.markup.pop_back();
    sect_.level--;
}

//------------------------------------------------

Location const&
AsciidocGenerator::
Writer::
getLocation(
    SymbolInfo const& I)
{
    if(I.DefLoc.has_value())
        return *I.DefLoc;
    if(! I.Loc.empty())
        return I.Loc[0];
    static Location const missing{};
    return missing;
}

llvm::StringRef
AsciidocGenerator::
Writer::
toString(TagTypeKind k) noexcept
{
    switch(k)
    {
    case TagTypeKind::TTK_Struct: return "struct";
    case TagTypeKind::TTK_Interface: return "__interface";
    case TagTypeKind::TTK_Union: return "union";
    case TagTypeKind::TTK_Class: return "class";
    case TagTypeKind::TTK_Enum: return "enum";
    default:
        llvm_unreachable("unknown TagTypeKind");
    }
}

//------------------------------------------------

std::unique_ptr<Generator>
makeAsciidocGenerator()
{
    return std::make_unique<AsciidocGenerator>();
}

} // mrdox
} // clang
