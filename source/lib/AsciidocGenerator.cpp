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
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <string>

//------------------------------------------------

using namespace llvm;

namespace clang {
namespace mrdox {

namespace {

//
// Asciidoc generation
//

std::string
genEmphasis(
    Twine const& t)
{
    return "*" + t.str() + "*";
}

std::string
genReferenceList(
    llvm::SmallVectorImpl<Reference> const& Refs)
{
    std::string Buffer;
    llvm::raw_string_ostream Stream(Buffer);
    for (const auto& R : Refs) {
        if (&R != Refs.begin())
            Stream << ", ";
        Stream << R.Name;
    }
    return Stream.str();
}

void
writeLine(
    Twine const& Text,
    raw_ostream& os)
{
    os << Text << "\n";
}

void
writeNewLine(
    raw_ostream& os)
{
    os << "\n";
}

// Write Asciidoc document header
void
document_header(
    Twine const& s,
    raw_ostream& os)
{
    os << "= " << s << "\n\n";
}

void
section(
    Twine const &Text,
    unsigned int level,
    raw_ostream& os)
{
    os <<
        std::string(level, '=') << " " <<
            Text << "\n"
        "\n";
}

void
writeFileDefinition(
    ClangDocContext const& CDCtx,
    const Location& L,
    raw_ostream& os)
{
    // VFALCO FIXME
#if 0
    if (!CDCtx.RepositoryUrl) {
        os << "*Defined at " << L.Filename << "#" << std::to_string(L.LineNumber)
            << "*";
    }
    else {
        os << "*Defined at [" << L.Filename << "#" << std::to_string(L.LineNumber)
            << "](" << StringRef{ *CDCtx.RepositoryUrl }
            << llvm::sys::path::relative_path(L.Filename) << "#"
            << std::to_string(L.LineNumber) << ")"
            << "*";
    }
    os << "\n";
    os << "\n";
#endif
}

void
writeDescription(
    CommentInfo const& I,
    raw_ostream& os)
{
#if 0
    if (I.Kind == "FullComment")
    {
        for (const auto& Child : I.Children)
            writeDescription(*Child, os);
    }
    else if (I.Kind == "ParagraphComment") {
        for (const auto& Child : I.Children)
            writeDescription(*Child, os);
        writeNewLine(os);
    }
    else if (I.Kind == "BlockCommandComment") {
        os << genEmphasis(I.Name);
        for (const auto& Child : I.Children)
            writeDescription(*Child, os);
    }
    else if (I.Kind == "InlineCommandComment") {
        os << genEmphasis(I.Name) << " " << I.Text;
    }
    else if (I.Kind == "ParamCommandComment") {
        std::string Direction = I.Explicit ? (" " + I.Direction).str() : "";
        os << genEmphasis(I.ParamName) << I.Text << Direction << "\n";
    }
    else if (I.Kind == "TParamCommandComment") {
        std::string Direction = I.Explicit ? (" " + I.Direction).str() : "";
        os << genEmphasis(I.ParamName) << I.Text << Direction << "\n";
    }
    else if (I.Kind == "VerbatimBlockComment") {
        for (const auto& Child : I.Children)
            writeDescription(*Child, os);
    }
    else if (I.Kind == "VerbatimBlockLineComment") {
        os << I.Text;
        writeNewLine(os);
    }
    else if (I.Kind == "VerbatimLineComment") {
        os << I.Text;
        writeNewLine(os);
    }
    else if (I.Kind == "HTMLStartTagComment") {
        if (I.AttrKeys.size() != I.AttrValues.size())
            return;
        std::string Buffer;
        llvm::raw_string_ostream Attrs(Buffer);
        for (unsigned Idx = 0; Idx < I.AttrKeys.size(); ++Idx)
            Attrs << " \"" << I.AttrKeys[Idx] << "=" << I.AttrValues[Idx] << "\"";

        std::string CloseTag = I.SelfClosing ? "/>" : ">";
        writeLine("<" + I.Name + Attrs.str() + CloseTag, os);
    }
    else if (I.Kind == "HTMLEndTagComment") {
        writeLine("</" + I.Name + ">", os);
    }
    else if (I.Kind == "TextComment") {
        os << I.Text;
    }
    else {
        os << "Unknown comment kind: " << I.Kind << ".\n";
    }
#endif
}

void
writeNameLink(
    StringRef const &CurrentPath,
    const Reference& R,
    llvm::raw_ostream& os)
{
    llvm::SmallString<64> Path = R.getRelativeFilePath(CurrentPath);
    // Paths in Markdown use POSIX separators.
    llvm::sys::path::native(Path, llvm::sys::path::Style::posix);
    llvm::sys::path::append(Path, llvm::sys::path::Style::posix,
        R.getFileBaseName() + ".adoc");
    os << "xref:" << Path << "#" << R.Name << "[" << R.Name << "]";
}

//------------------------------------------------
//
// EnumInfo
//
//------------------------------------------------

void
genMarkdown(
    const ClangDocContext& CDCtx,
    const EnumInfo& I,
    llvm::raw_ostream& os)
{
    if (I.Scoped)
        writeLine("| enum class " + I.Name + " |", os);
    else
        writeLine("| enum " + I.Name + " |", os);
    writeLine("--", os);

    std::string Buffer;
    llvm::raw_string_ostream Members(Buffer);
    if (!I.Members.empty())
        for (const auto& N : I.Members)
            Members << "| " << N.Name << " |\n";
    writeLine(Members.str(), os);
    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, os);

    for (const auto& C : I.Description)
        writeDescription(C, os);
}

//------------------------------------------------
//
// Functions
//
//------------------------------------------------

//------------------------------------------------
//
// FunctionInfo
//
//------------------------------------------------

std::string
makeDecl(
    FunctionInfo const& I)
{
    std::string s;
    llvm::raw_string_ostream os(s);
    if(I.Params.empty())
    {
        os << I.Name << "()";
        return s;
    }
    os << I.Name << "(" <<
        I.Params.front().Type.Name << " " <<
        I.Params.front().Name;
    for(std::size_t i = 1; i < I.Params.size(); ++i)
        os << ", " <<
            I.Params.front().Type.Name << " " <<
            I.Params.front().Name;
    os << ")";
    return s;
}

void
genMarkdown(
    ClangDocContext const& CDCtx,
    FunctionInfo const& I,
    llvm::raw_ostream& os)
{
    std::string Buffer = makeDecl(I);

    std::string Access = getAccessSpelling(I.Access).str();
    if(! Access.empty())
        Access.push_back(' ');
    os <<
        "|`" << Access << Buffer;

    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, os);

    os << "|";
    for (const auto& C : I.Description)
        writeDescription(C, os);
    os << "\n";
}

//------------------------------------------------

void
listNamespaces(
    ClangDocContext const& CDCtx,
    std::vector<Reference> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section("Namespaces", 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    os <<
        "|`";
    llvm::SmallString<64> BasePath = v.front().getRelativeFilePath("");
    writeNameLink(BasePath, v.front(), os);
    os << "`\n" <<
        "|\n";
    for(std::size_t i = 1; i < v.size(); ++i)
    {
        auto const& I = v[i];
        BasePath= I.getRelativeFilePath("");
        os <<
            "\n" <<
            "|`";
        writeNameLink(BasePath, I, os);
        os <<
            "`\n" <<
            "|\n";
    }
    os <<
        "|===\n" <<
        "\n";
}

void
listClasses(
    ClangDocContext const& CDCtx,
    std::vector<Reference> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section("Classes", 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    os <<
        "|`";
    llvm::SmallString<64> BasePath = v.front().getRelativeFilePath("");
    writeNameLink(BasePath, v.front(), os);
    os << "`\n" <<
        "|\n";
    for(std::size_t i = 1; i < v.size(); ++i)
    {
        auto const& I = v[i];
        BasePath= I.getRelativeFilePath("");
        os <<
            "\n" <<
            "|`";
        writeNameLink(BasePath, I, os);
        os <<
            "`\n" <<
            "|\n";
    }
    os <<
        "|===\n" <<
        "\n";
}

void
listFunctions(
    ClangDocContext const& CDCtx,
    llvm::StringRef label,
    FunctionList const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section(label, 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    os <<
        "|`" << v.front().name << "`\n" <<
        "|" << v.front().front().javadoc.brief <<
        "\n";
    for(std::size_t i = 1; i < v.size(); ++i)
    {
        auto const& f = v[i];
        os <<
            "\n" <<
            "|`" << f.name << "`\n" <<
            "|" << f.front().javadoc.brief <<
            "\n";
    }
    os <<
        "|===\n" <<
        "\n";
}

void
listConstants(
    ClangDocContext const& CDCtx,
    std::vector<EnumInfo> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section("Constants", 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    os <<
        "|`" << v.front().Name << "`\n" <<
        "|" << v.front().javadoc.brief <<
        "\n";
    for(std::size_t i = 1; i < v.size(); ++i)
    {
        auto const& I = v[i];
        os <<
            "\n" <<
            "|`" << I.Name << "`\n" <<
            "|" << I.javadoc.brief <<
            "\n";
    }
    os <<
        "|===\n" <<
        "\n";
}

void
listTypedefs(
    ClangDocContext const& CDCtx,
    std::vector<TypedefInfo> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section("Typedefs", 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    os <<
        "|`" << v.front().Name << "`\n" <<
        "|" << v.front().javadoc.brief <<
        "\n";
    for(std::size_t i = 1; i < v.size(); ++i)
    {
        auto const& I = v[i];
        os <<
            "\n" <<
            "|`" << I.Name << "`\n" <<
            "|" << I.javadoc.brief <<
            "\n";
    }
    os <<
        "|===\n" <<
        "\n";
}

void
listScope(
    ClangDocContext const& CDCtx,
    ScopeChildren const& scope,
    llvm::raw_ostream& os)
{
    listNamespaces(CDCtx, scope.Namespaces, os);
    listClasses(CDCtx, scope.Records, os);
    listFunctions(CDCtx, "Functions", scope.functions, os);
    listConstants(CDCtx, scope.Enums, os);
    listTypedefs(CDCtx, scope.Typedefs, os);
}


void
listFunction(
    ClangDocContext const& CDCtx,
    FunctionInfo const& I,
    llvm::raw_ostream& os)
{
    os <<
        "|`" << I.Name <<"`\n" <<
        "|" << I.javadoc.brief << "\n";
}

void
listFunctions(
    ClangDocContext const& CDCtx,
    llvm::StringRef label,
    std::vector<FunctionInfo> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section(label, 2, os);
    os <<
        "[cols=2]\n" <<
        "|===\n" <<
        "|Name\n" <<
        "|Description\n" <<
        "\n";
    if(! v.empty())
    {
        listFunction(CDCtx, v.front(), os);
        for(std::size_t i = 1; i < v.size(); ++i)
        {
            os << "\n";
            listFunction(CDCtx, v[i], os);
        }
    }
    os <<
        "|===\n" <<
        "\n";
}

// Write a complete FunctionInfo page
void
emitFunction(
    ClangDocContext const& CDCtx,
    FunctionInfo const& I,
    llvm::raw_ostream& os)
{
    os <<
        "== " << I.Name << "\n" <<
        I.javadoc.brief << "\n" <<
        "=== Synopsis\n"
        "[,cpp]\n"
        "----\n" <<
        makeDecl(I) << "\n" <<
        "----\n" <<
        "\n";
    if(! I.javadoc.desc.empty())
        os <<
            "=== Description\n" <<
            I.javadoc.desc;
}

//------------------------------------------------
//
// NamespaceInfo
//
//------------------------------------------------

void
makeNamespacePage(
    ClangDocContext const& CDCtx,
    NamespaceInfo const& I,
    llvm::raw_ostream& os)
{
    if(I.Name == "")
        section("(global namespace)", 1, os);
    else
        section("namespace " + I.Name, 1, os);

    if (!I.Description.empty())
    {
        for (const auto& C : I.Description)
            writeDescription(C, os);
        writeNewLine(os);
    }

    llvm::SmallString<64> BasePath = I.getRelativeFilePath("");

    listScope(CDCtx, I.Children, os);
}

//------------------------------------------------
//
// RecordInfo: class, struct
//
//------------------------------------------------

void
genMarkdown(
    ClangDocContext const& CDCtx,
    RecordInfo const& I,
    llvm::raw_ostream& os)
{
    document_header(I.Name, os);

    os <<
        I.javadoc.brief << "\n" <<
        "\n";

    section("Synopsis", 2, os);

    os << 
        "[,cpp]\n" <<
        "----\n" <<
        getTagType(I.TagType) << " " << I.Name << ";\n" <<
        "----\n" <<
        "\n";

    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, os);

    std::string Parents = genReferenceList(I.Parents);
    std::string VParents = genReferenceList(I.VirtualParents);
    if (!Parents.empty() || !VParents.empty())
    {
        if (Parents.empty())
            writeLine("Inherits from " + VParents, os);
        else if (VParents.empty())
            writeLine("Inherits from " + Parents, os);
        else
            writeLine("Inherits from " + Parents + ", " + VParents, os);
        writeNewLine(os);
    }

    // VFALCO STATIC MEMBER FUNCTIONS

    //listFunctions(CDCtx, "Member Functions", I.Children.functions, os);
    listScope(CDCtx, I.Children, os);

    if(! I.javadoc.desc.empty())
    {
        section("Description", 2, os);
        os << I.javadoc.desc << "\n" <<
            "\n";
    }
}

//------------------------------------------------
//
// TypedefInfo
//
//------------------------------------------------

void
genMarkdown(
    ClangDocContext const &CDCtx,
    TypedefInfo const& I,
    llvm::raw_ostream& os)
{
    // TODO support typedefs in markdown.
}

//------------------------------------------------

void
serializeReference(
    llvm::raw_fd_ostream& os,
    Index& I,
    int Level)
{
    // Write out the heading level starting at ##
    os << "##" << std::string(Level, '#') << " ";
    writeNameLink("", I, os);
    os << "\n";
}

// emit all_files.adoc
llvm::Error
serializeIndex(
    ClangDocContext& CDCtx)
{
    std::error_code FileErr;
    llvm::SmallString<128> FilePath;
    llvm::sys::path::native(CDCtx.OutDirectory, FilePath);
    llvm::sys::path::append(FilePath, "all_files.adoc");
    llvm::raw_fd_ostream os(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());

    CDCtx.Idx.sort();
    os << "# All Files";
    if (!CDCtx.ProjectName.empty())
        os << " for " << CDCtx.ProjectName;
    os << "\n";

    for (auto C : CDCtx.Idx.Children)
        serializeReference(os, C, 0);

    return llvm::Error::success();
}

// emit index.adoc
llvm::Error
genIndex(
    ClangDocContext& CDCtx)
{
    std::error_code FileErr;
    llvm::SmallString<128> FilePath;
    llvm::sys::path::native(CDCtx.OutDirectory, FilePath);
    llvm::sys::path::append(FilePath, "index.adoc");
    llvm::raw_fd_ostream os(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());
    CDCtx.Idx.sort();
    os << "# " << CDCtx.ProjectName << " C/C++ Reference\n";
    for (auto C : CDCtx.Idx.Children) {
        if (!C.Children.empty()) {
            const char* Type;
            switch (C.RefType) {
            case InfoType::IT_namespace:
                Type = "Namespace";
                break;
            case InfoType::IT_record:
                Type = "Type";
                break;
            case InfoType::IT_enum:
                Type = "Enum";
                break;
            case InfoType::IT_function:
                Type = "Function";
                break;
            case InfoType::IT_typedef:
                Type = "Typedef";
                break;
            case InfoType::IT_default:
                Type = "Other";
            }
            os << "* " << Type << ": [" << C.Name << "](";
            if (!C.Path.empty())
                os << C.Path << "/";
            os << C.Name << ")\n";
        }
    }
    return llvm::Error::success();
}

//------------------------------------------------
//
// Generator
//
//------------------------------------------------

/// Generator for Markdown documentation.
class AsciidocGenerator : public Generator
{
public:
    static char const* Format;

    llvm::Error
    generateDocs(
        StringRef RootDir,
        llvm::StringMap<std::unique_ptr<mrdox::Info>> Infos,
        ClangDocContext const& CDCtx) override;

    llvm::Error
    createResources(
        ClangDocContext& CDCtx) override;

    llvm::Error
    generateDocForInfo(
        Info* I,
        llvm::raw_ostream& os,
        ClangDocContext const& CDCtx) override;
};

char const*
AsciidocGenerator::Format = "adoc";

llvm::Error
AsciidocGenerator::
generateDocs(
    StringRef RootDir,
    llvm::StringMap<std::unique_ptr<mrdox::Info>> Infos,
    ClangDocContext const& CDCtx)
{
    // Track which directories we already tried to create.
    llvm::StringSet<> CreatedDirs;

    // Collect all output by file name and create the necessary directories.
    llvm::StringMap<std::vector<mrdox::Info*>> FileToInfos;
    for (const auto& Group : Infos) {
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
            if (llvm::Error Err = generateDocForInfo(Info, InfoOS, CDCtx)) {
                return Err;
            }
        }
    }

    return llvm::Error::success();
}

llvm::Error
AsciidocGenerator::
generateDocForInfo(
    Info* I,
    llvm::raw_ostream& os,
    const ClangDocContext& CDCtx)
{
    switch (I->IT)
    {
    case InfoType::IT_namespace:
        makeNamespacePage(CDCtx, *static_cast<clang::mrdox::NamespaceInfo*>(I), os);
        break;
    case InfoType::IT_record:
        genMarkdown(CDCtx, *static_cast<clang::mrdox::RecordInfo*>(I), os);
        break;
    case InfoType::IT_enum:
        genMarkdown(CDCtx, *static_cast<clang::mrdox::EnumInfo*>(I), os);
        break;
    case InfoType::IT_function:
        genMarkdown(CDCtx, *static_cast<clang::mrdox::FunctionInfo*>(I), os);
        break;
    case InfoType::IT_typedef:
        genMarkdown(CDCtx, *static_cast<clang::mrdox::TypedefInfo*>(I), os);
        break;
    case InfoType::IT_default:
        return createStringError(llvm::inconvertibleErrorCode(),
            "unexpected InfoType");
    }
    return llvm::Error::success();
}

llvm::Error
AsciidocGenerator::
createResources(
    ClangDocContext& CDCtx)
{
    // Write an all_files.adoc
    auto Err = serializeIndex(CDCtx);
    if (Err)
        return Err;

    // Generate the index page.
    Err = genIndex(CDCtx);
    if (Err)
        return Err;

    return llvm::Error::success();
}

} // (anon)

static
GeneratorRegistry::
Add<AsciidocGenerator> Asciidoc(
    AsciidocGenerator::Format,
    "Generator for Asciidoc output.");

// This anchor is used to force the linker to link in the generated object
// file and thus register the generator.
volatile int AsciidocGeneratorAnchorSource = 0;

} // namespace mrdox
} // namespace clang
