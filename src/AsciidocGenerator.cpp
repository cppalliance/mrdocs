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

namespace mrdox {

} // mrdox

//------------------------------------------------

using namespace llvm;

namespace clang {
namespace doc {

// Asciidoc generation

// Return t as fixed-width Asciidoc text
std::string
fixed(
    Twine const& t)
{
    return "`" + t.str() + "`";
}

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
    raw_ostream& OS)
{
    OS << Text << "\n";
}

void
writeNewLine(
    raw_ostream& OS)
{
    OS << "\n";
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
    raw_ostream& OS)
{
    OS << std::string(level, '=') + " " + Text << "\n";
}

void
writeFileDefinition(
    ClangDocContext const& CDCtx,
    const Location& L,
    raw_ostream& OS)
{
    // VFALCO FIXME
#if 0
    if (!CDCtx.RepositoryUrl) {
        OS << "*Defined at " << L.Filename << "#" << std::to_string(L.LineNumber)
            << "*";
    }
    else {
        OS << "*Defined at [" << L.Filename << "#" << std::to_string(L.LineNumber)
            << "](" << StringRef{ *CDCtx.RepositoryUrl }
            << llvm::sys::path::relative_path(L.Filename) << "#"
            << std::to_string(L.LineNumber) << ")"
            << "*";
    }
    OS << "\n";
    OS << "\n";
#endif
}

void
writeDescription(
    CommentInfo const& I,
    raw_ostream& OS)
{
    if (I.Kind == "FullComment")
    {
        for (const auto& Child : I.Children)
            writeDescription(*Child, OS);
    }
    else if (I.Kind == "ParagraphComment") {
        for (const auto& Child : I.Children)
            writeDescription(*Child, OS);
        writeNewLine(OS);
    }
    else if (I.Kind == "BlockCommandComment") {
        OS << genEmphasis(I.Name);
        for (const auto& Child : I.Children)
            writeDescription(*Child, OS);
    }
    else if (I.Kind == "InlineCommandComment") {
        OS << genEmphasis(I.Name) << " " << I.Text;
    }
    else if (I.Kind == "ParamCommandComment") {
        std::string Direction = I.Explicit ? (" " + I.Direction).str() : "";
        OS << genEmphasis(I.ParamName) << I.Text << Direction << "\n";
    }
    else if (I.Kind == "TParamCommandComment") {
        std::string Direction = I.Explicit ? (" " + I.Direction).str() : "";
        OS << genEmphasis(I.ParamName) << I.Text << Direction << "\n";
    }
    else if (I.Kind == "VerbatimBlockComment") {
        for (const auto& Child : I.Children)
            writeDescription(*Child, OS);
    }
    else if (I.Kind == "VerbatimBlockLineComment") {
        OS << I.Text;
        writeNewLine(OS);
    }
    else if (I.Kind == "VerbatimLineComment") {
        OS << I.Text;
        writeNewLine(OS);
    }
    else if (I.Kind == "HTMLStartTagComment") {
        if (I.AttrKeys.size() != I.AttrValues.size())
            return;
        std::string Buffer;
        llvm::raw_string_ostream Attrs(Buffer);
        for (unsigned Idx = 0; Idx < I.AttrKeys.size(); ++Idx)
            Attrs << " \"" << I.AttrKeys[Idx] << "=" << I.AttrValues[Idx] << "\"";

        std::string CloseTag = I.SelfClosing ? "/>" : ">";
        writeLine("<" + I.Name + Attrs.str() + CloseTag, OS);
    }
    else if (I.Kind == "HTMLEndTagComment") {
        writeLine("</" + I.Name + ">", OS);
    }
    else if (I.Kind == "TextComment") {
        OS << I.Text;
    }
    else {
        OS << "Unknown comment kind: " << I.Kind << ".\n";
    }
}

void
writeNameLink(
    StringRef const &CurrentPath,
    const Reference& R,
    llvm::raw_ostream& OS)
{
    llvm::SmallString<64> Path = R.getRelativeFilePath(CurrentPath);
    // Paths in Markdown use POSIX separators.
    llvm::sys::path::native(Path, llvm::sys::path::Style::posix);
    llvm::sys::path::append(Path, llvm::sys::path::Style::posix,
        R.getFileBaseName() + ".adoc");
    OS << "xref:" << Path << "#" << R.Name << "[" << R.Name << "]";
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
    llvm::raw_ostream& OS)
{
    if (I.Scoped)
        writeLine("| enum class " + I.Name + " |", OS);
    else
        writeLine("| enum " + I.Name + " |", OS);
    writeLine("--", OS);

    std::string Buffer;
    llvm::raw_string_ostream Members(Buffer);
    if (!I.Members.empty())
        for (const auto& N : I.Members)
            Members << "| " << N.Name << " |\n";
    writeLine(Members.str(), OS);
    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, OS);

    for (const auto& C : I.Description)
        writeDescription(C, OS);
}

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
    os << "(" <<
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
    llvm::raw_ostream& OS)
{
    std::string Buffer = makeDecl(I);

    std::string Access = getAccessSpelling(I.Access).str();
    if(! Access.empty())
        Access.push_back(' ');
    OS <<
        "|`" << Access << Buffer;

    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, OS);

    OS << "|";
    for (const auto& C : I.Description)
        writeDescription(C, OS);
    OS << "\n";
}

//------------------------------------------------
//
// NamespaceInfo
//
//------------------------------------------------

void
listFunction(
    ClangDocContext const& CDCtx,
    FunctionInfo const& fi,
    llvm::raw_ostream& os)
{
    os <<
        "|`" << makeDecl(fi) <<"`\n" <<
        "|" << fi.javadoc.brief << "\n";
}

void
listFunctions(
    ClangDocContext const& CDCtx,
    std::vector<FunctionInfo> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;

    section("Functions", 2, os);
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

void
genMarkdown(
    ClangDocContext const& CDCtx,
    NamespaceInfo const& I,
    llvm::raw_ostream& OS)
{
    if (I.Name == "")
        section("Global Namespace", 1, OS);
    else
        section("namespace " + I.Name, 1, OS);
    writeNewLine(OS);

    if (!I.Description.empty())
    {
        for (const auto& C : I.Description)
            writeDescription(C, OS);
        writeNewLine(OS);
    }

    llvm::SmallString<64> BasePath = I.getRelativeFilePath("");

    if (!I.Children.Namespaces.empty())
    {
        section("Namespaces", 2, OS);
        for (const auto& R : I.Children.Namespaces) {
            OS << "* ";
            writeNameLink(BasePath, R, OS);
            OS << "\n";
        }
        writeNewLine(OS);
    }

    if (!I.Children.Records.empty())
    {
        section("Types", 2, OS);
        for (const auto& R : I.Children.Records) {
            OS << "* ";
            writeNameLink(BasePath, R, OS);
            OS << "\n";
        }
        writeNewLine(OS);
    }

    listFunctions(CDCtx, I.Children.Functions, OS);

    if (!I.Children.Enums.empty()) {
        section("Enums", 2, OS);
        for (const auto& E : I.Children.Enums)
            genMarkdown(CDCtx, E, OS);
        writeNewLine(OS);
    }
}

//------------------------------------------------
//
// RecordInfo: class, struct
//
//------------------------------------------------

void
genMarkdown(
    ClangDocContext const &CDCtx,
    const RecordInfo& I,
    llvm::raw_ostream& OS)
{
//if(I.Name == "circular_buffer") __debugbreak();
    document_header(I.Name, OS);

    // VFALCO Calculate this robustly
    std::vector<std::unique_ptr<
        CommentInfo>> const* javadoc = nullptr;

    if(! I.Description.empty())
    {
        javadoc = &I.Description.front().Children;
        if(javadoc->size() > 0)
            writeDescription(*javadoc->front(), OS);
        writeNewLine(OS);
    }

    section("Synopsis", 2, OS);

    OS << 
        "[,cpp]\n" <<
        "----\n" <<
        getTagType(I.TagType) << " " << I.Name << ";\n" <<
        "----\n" <<
        "\n";

    if (I.DefLoc)
        writeFileDefinition(CDCtx, *I.DefLoc, OS);

    std::string Parents = genReferenceList(I.Parents);
    std::string VParents = genReferenceList(I.VirtualParents);
    if (!Parents.empty() || !VParents.empty())
    {
        if (Parents.empty())
            writeLine("Inherits from " + VParents, OS);
        else if (VParents.empty())
            writeLine("Inherits from " + Parents, OS);
        else
            writeLine("Inherits from " + Parents + ", " + VParents, OS);
        writeNewLine(OS);
    }

    if (!I.Members.empty())
    {
        section("Data Members", 2, OS);
        for (const auto& Member : I.Members) {
            std::string Access = getAccessSpelling(Member.Access).str();
            if (Access != "")
                writeLine(Access + " " + Member.Type.Name + " " + Member.Name, OS);
            else
                writeLine(Member.Type.Name + " " + Member.Name, OS);
        }
        writeNewLine(OS);
    }

    if (!I.Children.Records.empty())
    {
        section("Types", 2, OS);
        for (const auto& R : I.Children.Records)
            writeLine(R.Name, OS);
        writeNewLine(OS);
    }

    // VFALCO STATIC MEMBER FUNCTIONS

    if (!I.Children.Functions.empty())
    {
        section("Member Functions", 2, OS);
        listFunctions(CDCtx, I.Children.Functions, OS);
        /*
        OS <<
            "[cols=2*]\n"
            "!===\n" <<
            "|Name\n"
            "|Description\n";
        for (const auto& F : I.Children.Functions)
            genMarkdown(CDCtx, F, OS);
        OS <<
            "!===\n"
            "\n";
        */
    }

    if (!I.Children.Enums.empty())
    {
        section("Enums", 2, OS);
        for (const auto& E : I.Children.Enums)
            genMarkdown(CDCtx, E, OS);
        writeNewLine(OS);
    }

    if( javadoc &&
        javadoc->size() > 1)
    {
        section("Description", 2, OS);
        for(std::size_t i = 0; i < javadoc->size(); ++i)
            writeDescription(*(*javadoc)[i], OS);
        writeNewLine(OS);
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
    llvm::raw_ostream& OS)
{
    // TODO support typedefs in markdown.
}

//------------------------------------------------

void
serializeReference(
    llvm::raw_fd_ostream& OS,
    Index& I,
    int Level)
{
    // Write out the heading level starting at ##
    OS << "##" << std::string(Level, '#') << " ";
    writeNameLink("", I, OS);
    OS << "\n";
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
    llvm::raw_fd_ostream OS(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());

    CDCtx.Idx.sort();
    OS << "# All Files";
    if (!CDCtx.ProjectName.empty())
        OS << " for " << CDCtx.ProjectName;
    OS << "\n";

    for (auto C : CDCtx.Idx.Children)
        serializeReference(OS, C, 0);

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
    llvm::raw_fd_ostream OS(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());
    CDCtx.Idx.sort();
    OS << "# " << CDCtx.ProjectName << " C/C++ Reference\n";
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
            OS << "* " << Type << ": [" << C.Name << "](";
            if (!C.Path.empty())
                OS << C.Path << "/";
            OS << C.Name << ")\n";
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
        llvm::StringMap<std::unique_ptr<doc::Info>> Infos,
        ClangDocContext const& CDCtx) override;

    llvm::Error
    createResources(
        ClangDocContext& CDCtx) override;

    llvm::Error
    generateDocForInfo(
        Info* I,
        llvm::raw_ostream& OS,
        ClangDocContext const& CDCtx) override;
};

char const*
AsciidocGenerator::Format = "adoc";

llvm::Error
AsciidocGenerator::
generateDocs(
    StringRef RootDir,
    llvm::StringMap<std::unique_ptr<doc::Info>> Infos,
    ClangDocContext const& CDCtx)
{
    // Track which directories we already tried to create.
    llvm::StringSet<> CreatedDirs;

    // Collect all output by file name and create the necessary directories.
    llvm::StringMap<std::vector<doc::Info*>> FileToInfos;
    for (const auto& Group : Infos) {
        doc::Info* Info = Group.getValue().get();

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
    llvm::raw_ostream& OS,
    const ClangDocContext& CDCtx)
{
    switch (I->IT)
    {
    case InfoType::IT_namespace:
        genMarkdown(CDCtx, *static_cast<clang::doc::NamespaceInfo*>(I), OS);
        break;
    case InfoType::IT_record:
        genMarkdown(CDCtx, *static_cast<clang::doc::RecordInfo*>(I), OS);
        break;
    case InfoType::IT_enum:
        genMarkdown(CDCtx, *static_cast<clang::doc::EnumInfo*>(I), OS);
        break;
    case InfoType::IT_function:
        genMarkdown(CDCtx, *static_cast<clang::doc::FunctionInfo*>(I), OS);
        break;
    case InfoType::IT_typedef:
        genMarkdown(CDCtx, *static_cast<clang::doc::TypedefInfo*>(I), OS);
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

static
GeneratorRegistry::
Add<AsciidocGenerator> Asciidoc(
    AsciidocGenerator::Format,
    "Generator for Asciidoc output.");

// This anchor is used to force the linker to link in the generated object
// file and thus register the generator.
volatile int AsciidocGeneratorAnchorSource = 0;

} // namespace doc
} // namespace clang
