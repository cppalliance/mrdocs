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
#include <clang/Basic/Specifiers.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// AsciidocGenerator
//
//------------------------------------------------

#if 0
bool
AsciidocGenerator::
build(
    llvm::StringRef rootPath,
    Corpus const& corpus,
    Config const& config,
    Reporter& R) const
{
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
#endif
    return true;
}
#endif

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
    Writer w(corpus, config, R);
    w.writeOne(os);
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
    Writer w(corpus, config, R);
    w.writeOne(os);
    return true;
}

//------------------------------------------------
//
// Writer
//
//------------------------------------------------

void
AsciidocGenerator::
Writer::
write(
    llvm::StringRef rootDir)
{
}

void
AsciidocGenerator::
Writer::
writeOne(
    llvm::raw_ostream& os)
{
    os_ = &os;
    openSection("Reference");
    writeAllSymbols();
    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
writeAllSymbols()
{
    for(auto const& id : corpus_.allSymbols)
    {
        auto const& I = corpus_.get<Info>(id);
        switch(I.IT)
        {
        case InfoType::IT_record:
            write(static_cast<RecordInfo const&>(I));
            break;
        case InfoType::IT_function:
            write(static_cast<FunctionInfo const&>(I));
            break;
        default:
            break;
        }
    }
}

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
        t.w.write(t, os);
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
        t.w.write(t, os);
        return os;
    }
};

void
AsciidocGenerator::
Writer::
write(
    RecordInfo const& I)
{
    openSection(I.Name);
    *os_ << I.javadoc.brief << "\n\n";

    // Synopsis
    openSection("Synopsis");
    *os_ <<
        "Located in <" << getLocation(I).Filename << ">\n" <<
        "[,cpp]\n"
        "----\n" <<
        toString(I.TagType) << " " << I.Name;
    if(! I.Bases.empty())
    {
        *os_ << "\n    : ";
        writeBase(I.Bases[0]);
        for(std::size_t i = 1; i < I.Bases.size(); ++i)
        {
            *os_ << "\n    , ";
            writeBase(I.Bases[i]);
        }
    }
    *os_ <<
        ";\n"
        "----\n";
    closeSection();

    // Description
    if(! I.javadoc.desc.empty())
    {
        *os_ << "\n";
        openSection("Description");
        *os_ << I.javadoc.desc << "\n";
        closeSection();
    }

    // Data Members

    // Member Functions
    {
        std::vector<FunctionInfo const*> v;
        v.reserve(I.Children.Functions.size());
        for(auto const& ref : I.Children.Functions)
        {
            auto const& J = corpus_.get<FunctionInfo>(ref.USR);
            if(J.Access == AccessSpecifier::AS_public)
                v.push_back(&J);
        }
        if(! v.empty())
        {
            *os_ << "\n";
            openSection("Member Functions");
            *os_ <<
            "[cols=2]\n" <<
                "|===\n" <<
                "|Name\n" <<
                "|Description\n" <<
                "\n";
            for(auto const& J : v)
                *os_ <<
                    "|`" << J->Name << "`\n" <<
                    "| " << J->javadoc.brief << "\n";
            *os_ <<
                "|===\n" <<
                "\n";
            closeSection();                
        }
    }

    closeSection();
}

void
AsciidocGenerator::
Writer::
writeBase(
    BaseRecordInfo const& I)
{
    *os_ << clang::getAccessSpelling(I.Access) << " " << I.Name;
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
write(
    FunctionInfo const& I)
{
    openSection(I.Name);
    *os_ << I.javadoc.brief << "\n\n";

    // Synopsis
    openSection("Synopsis");
    *os_ <<
        "Located in <" << getLocation(I).Filename << ">\n" <<
        "[,cpp]\n"
        "----\n";

    // params
    if(! I.Params.empty())
    {
        *os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "(\n"
            "    " << formalParam(I.Params[0]);
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            *os_ << ",\n"
                "    " << formalParam(I.Params[i]);
        }
        *os_ << ");\n";
    }
    else
    {
        *os_ <<
            typeName(I.ReturnType) << '\n' <<
            I.Name << "();" << "\n";
    }

    *os_ <<
        "----\n";
    closeSection();

    if(! I.javadoc.desc.empty())
    {
        *os_ << "\n";
        openSection("Description");
        *os_ << I.javadoc.desc << "\n";
        closeSection();
    }

    closeSection();
}

//------------------------------------------------

void
AsciidocGenerator::
Writer::
write(
    FormalParam const& t,
    llvm::raw_ostream& os)
{
    assert(&os == os_);
    auto const& I = t.I;
    *os_ << I.Type.Name << ' ' << I.Name;
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
write(
    TypeName const& t,
    llvm::raw_ostream& os)
{
    assert(&os == os_);
    if(t.I.Type.USR == EmptySID)
    {
        *os_ << t.I.Type.Name;
        return;
    }
    auto p = corpus_.find<RecordInfo>(t.I.Type.USR);
    if(p != nullptr)
    {
        // VFALCO add namespace qualifiers if I is in
        //        a different namesapce
        *os_ << p->Path << "::" << p->Name;
        return;
    }
    auto const& T = t.I.Type;
    *os_ << T.Path << "::" << T.Name;
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
openSection(
    llvm::StringRef name)
{
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    *os_ << sect_.markup << ' ' << name << "\n\n";
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
//------------------------------------------------
//------------------------------------------------

#if 0

//
// Asciidoc generation
//

std::string
genEmphasis(
    llvm::Twine const& t)
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
    Config const& config,
    const Location& L,
    raw_ostream& os)
{
    // VFALCO FIXME
#if 0
    if (!config.RepositoryUrl) {
        os << "*Defined at " << L.Filename << "#" << std::to_string(L.LineNumber)
            << "*";
    }
    else {
        os << "*Defined at [" << L.Filename << "#" << std::to_string(L.LineNumber)
            << "](" << StringRef{ *config.RepositoryUrl }
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
    //llvm::sys::path::append(Path, llvm::sys::path::Style::posix, R.getFileBaseName() + ".adoc");
    llvm::sys::path::append(Path, llvm::sys::path::Style::posix, R.Name + ".adoc");
    os << "xref:" << Path << "#" << R.Name << "[" << R.Name << "]";
}

//------------------------------------------------
//
// EnumInfo
//
//------------------------------------------------

void
genMarkdown(
    const Config& config,
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
        writeFileDefinition(config, *I.DefLoc, os);

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
    Config const& config,
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
        writeFileDefinition(config, *I.DefLoc, os);

    os << "|";
    for (const auto& C : I.Description)
        writeDescription(C, os);
    os << "\n";
}

//------------------------------------------------

void
listNamespaces(
    Config const& config,
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
    Config const& config,
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
    Config const& config,
    llvm::StringRef label,
    std::vector<Reference> const& v,
    llvm::raw_ostream& os)
{
    if(v.empty())
        return;
#if 0
    FunctionInfo const* I;
    section(label, 2, os);
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
        auto const& f = v[i];
        os <<
            "\n" <<
            "|`" << f.Name << "`\n" <<
            "|" << f.javadoc.brief <<
            "\n";
    }
    os <<
        "|===\n" <<
        "\n";
#endif
}

void
listConstants(
    Config const& config,
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
    Config const& config,
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
    Config const& config,
    Scope const& scope,
    llvm::raw_ostream& os)
{
    listNamespaces(config, scope.Namespaces, os);
    listClasses(config, scope.Records, os);
    listFunctions(config, "Functions", scope.Functions, os);
    listConstants(config, scope.Enums, os);
    listTypedefs(config, scope.Typedefs, os);
}


void
listFunction(
    Config const& config,
    FunctionInfo const& I,
    llvm::raw_ostream& os)
{
    os <<
        "|`" << I.Name <<"`\n" <<
        "|" << I.javadoc.brief << "\n";
}

// Write a complete FunctionInfo page
void
emitFunction(
    Config const& config,
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
    Config const& config,
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

    listScope(config, I.Children, os);
}

//------------------------------------------------
//
// RecordInfo: class, struct
//
//------------------------------------------------

void
genMarkdown(
    Config const& config,
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
        clang::TypeWithKeyword::getTagTypeKindName(I.TagType) << " " << I.Name << ";\n" <<
        "----\n" <<
        "\n";

    if (I.DefLoc)
        writeFileDefinition(config, *I.DefLoc, os);

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

    //listFunctions(config, "Member Functions", I.Children.functions, os);
    listScope(config, I.Children, os);

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
    Config const &config,
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
    Config& config,
    Corpus& corpus)
{
    std::error_code FileErr;
    llvm::SmallString<128> FilePath;
    llvm::sys::path::native(config.OutDirectory, FilePath);
    llvm::sys::path::append(FilePath, "all_files.adoc");
    llvm::raw_fd_ostream os(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());

    corpus.Idx.sort();
    os << "# All Files";
    if (!config.ProjectName.empty())
        os << " for " << config.ProjectName;
    os << "\n";

    for (auto C : corpus.Idx.Children)
        serializeReference(os, C, 0);

    return llvm::Error::success();
}

// emit index.adoc
llvm::Error
genIndex(
    Config& config,
    Corpus& corpus)
{
    std::error_code FileErr;
    llvm::SmallString<128> FilePath;
    llvm::sys::path::native(config.OutDirectory, FilePath);
    llvm::sys::path::append(FilePath, "index.adoc");
    llvm::raw_fd_ostream os(FilePath, FileErr, llvm::sys::fs::OF_None);
    if (FileErr)
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "error creating index file: " +
            FileErr.message());
    corpus.Idx.sort();
    os << "# " << config.ProjectName << " C/C++ Reference\n";
    for (auto C : corpus.Idx.Children) {
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

#endif

//------------------------------------------------

std::unique_ptr<Generator>
makeAsciidocGenerator()
{
    return std::make_unique<AsciidocGenerator>();
}

} // mrdox
} // clang
