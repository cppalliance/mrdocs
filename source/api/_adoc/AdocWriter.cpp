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

#include "AdocWriter.hpp"
#include "ConfigImpl.hpp"
#include "Support/Validate.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Metadata/Overloads.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

namespace clang {
namespace mrdox {
namespace adoc {

struct AdocWriter::Key
{
    Options& opt;

    explicit
    Key(
        Options& opt_) noexcept
        : opt(opt_)
    {
    }
};

struct AdocWriter::GenKey
{
    Options& opt;

    explicit
    GenKey(
        Options& opt_)
        : opt(opt_)
    {
    }
};

} // adoc
} // mrdox
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::AdocWriter::Key>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::AdocWriter::Key& opt_)
    {
        auto& opt= opt_.opt;
        io.mapOptional("safe-names",  opt.safe_names);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::AdocWriter::GenKey>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::AdocWriter::GenKey& opt)
    {
        clang::mrdox::adoc::AdocWriter::Key k(opt.opt);

        io.mapOptional("adoc",  k);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::AdocWriter::Options>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::AdocWriter::Options& opt)
    {
        clang::mrdox::adoc::AdocWriter::GenKey k(opt);
        io.mapOptional("generator",  k);
    }
};

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------
//
// AdocWriter
//
//------------------------------------------------

struct AdocWriter::FormalParam
{
    FieldTypeInfo const& I;
    AdocWriter& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        FormalParam const& t)
    {
        os << t.I.Type.Name;
        if(! t.I.Name.empty())
            os << ' ' << t.I.Name;
        return os;
    }
};

struct AdocWriter::TypeName
{
    TypeInfo const& I;
    Corpus const& corpus;
    AdocWriter& w;

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        TypeName const& t)
    {
        if(t.I.Type.id == globalNamespaceID)
        {
            os << t.I.Type.Name;
            return os;
        }
        // VFALCO This is broken
#if 0
        if(auto J = t.corpus.find(t.I.Type.id))
        {
            // VFALCO add namespace qualifiers if I is in
            //        a different namesapce
            os << J->Name << "::" << T.Name;
            return os;
        }
#endif
        auto const& T = t.I.Type;
        os << T.Name << "::" << T.Name;
        return os;
    }
};

//------------------------------------------------
//
// AdocWriter
//
//------------------------------------------------

AdocWriter::
AdocWriter(
    llvm::raw_ostream& os,
    SafeNames const& names,
    Corpus const& corpus,
    Reporter& R) noexcept
    : names_(names)
    , os_(os)
    , corpus_(corpus)
    , R_(R)
{
}

llvm::Error
AdocWriter::
init()
{
    {
        llvm::yaml::Input yin(
            corpus_.config().configYaml().first,
                this, ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return makeError(ec);
    }
    {
        llvm::yaml::Input yin(
            corpus_.config().configYaml().second,
                this, ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> options_;
        if(auto ec = yin.error())
            return makeError(ec);
    }

    return llvm::Error::success();
}

//------------------------------------------------

/*  Write a namespace.

    This will list individual symbols by group.
*/
void
AdocWriter::
write(
    NamespaceInfo const& I)
{
}

//------------------------------------------------

auto
AdocWriter::
formalParam(
    FieldTypeInfo const& t) ->
        FormalParam
{
    return FormalParam{ t, *this };
}

//------------------------------------------------

/*  Write a class/union/struct.

    This will show the synopsis, description, and
    tables for members which link to individual sections.
*/
void
AdocWriter::
write(
    RecordInfo const& I)
{
    beginSection(I);

    // Brief
    writeBrief(I.javadoc);

    // Synopsis
    beginSection("Synopsis");

    // Location
    writeLocation(I);

    // Declaration
    os_ <<
        "\n" <<
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
    endSection();

    // Description
    writeDescription(I.javadoc);

    // Nested Types
    writeNestedTypes(
        "Types",
        I.Children.Typedefs,
        AccessSpecifier::AS_public);

    // Data Members
    writeDataMembers(
        "Data Members",
        I.Members,
        AccessSpecifier::AS_public);

    // Member Functions
    writeFunctionOverloads(
        "Member Functions",
        makeOverloadsSet(corpus_, I.Children,
            AccessSpecifier::AS_public));

    // Data Members (protected)
    writeDataMembers(
        "Protected Data Members",
        I.Members,
        AccessSpecifier::AS_protected);

    // Member Functiosn (protected)
    writeFunctionOverloads(
        "Protected Member Functions",
        makeOverloadsSet(corpus_, I.Children,
                AccessSpecifier::AS_protected));

    // Data Members (private)
    writeDataMembers(
        "Private Data Members",
        I.Members,
        AccessSpecifier::AS_private);

    // Member Functions (private)
    writeFunctionOverloads(
        "Private Member Functions",
        makeOverloadsSet(corpus_, I.Children,
            AccessSpecifier::AS_private));

    endSection();
}

void
AdocWriter::
write(
    FunctionInfo const& I)
{
    beginSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    // Synopsis
    beginSection("Synopsis");

    writeLocation(I);

    os_ <<
        "\n"
        "[,cpp]\n"
        "----\n";
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
    endSection();

    // Description
    writeDescription(I.javadoc);

    endSection();
}

void
AdocWriter::
write(
    TypedefInfo const& I)
{
    beginSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    writeLocation(I);

    // Description
    writeDescription(I.javadoc);

    endSection();
}

void
AdocWriter::
write(
    EnumInfo const& I)
{
    beginSection(I.Name);

    // Brief
    writeBrief(I.javadoc);

    writeLocation(I);

    // Description
    writeDescription(I.javadoc);

    endSection();
}

void
AdocWriter::
write(
    VariableInfo const& I)
{
}


//------------------------------------------------

llvm::StringRef
AdocWriter::
linkFor(
    Info const& I)
{
    static thread_local std::string temp;
    temp.clear();
    llvm::raw_string_ostream os(temp);
    os << "xref:#" <<
        names_.get(I.id) << "[" <<
        I.Name << "]";
    return temp;
}

llvm::StringRef
AdocWriter::
linkFor(
    Info const& P,
    OverloadInfo const& I)
{
    static thread_local std::string temp;
    temp.clear();
    llvm::raw_string_ostream os(temp);
    std::string s;
    os << "xref:#" <<
        names_.getOverload(P, I.Name, '-', s) <<
        "[" << I.Name << "]";
    return temp;
}

void
AdocWriter::
writeLinkFor(OverloadInfo const& I)
{
    std::string s = names_.get(I.Parent->id).str();
    
    os_ << "xref:#" <<
        names_.get(I.Parent->id) <<
        '-' << I.Name << "[" <<
        I.Name << "]";
}

void
AdocWriter::
writeLinkFor(Info const& I)
{
    os_ << "xref:#" <<
        names_.get(I.id) << "[" <<
        I.Name << "]";
}

void
AdocWriter::
writeBase(
    BaseRecordInfo const& I)
{
    os_ << clang::getAccessSpelling(I.Access) << " " << I.Name;
}

void
AdocWriter::
writeFunctionOverloads(
    llvm::StringRef sectionName,
    OverloadsSet const& set)
{
    if(set.list.empty())
        return;
    beginSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(auto const& J : set.list)
    {
        os_ <<
            "|`" << linkFor(*J.list.front()) << "`\n" <<
            "|";
        if(! J.list.empty())
        {
            for(auto const& K : J.list)
            {
                writeBrief(K->javadoc, false);
                os_ << '\n';
            }
        }
        else
        {
            os_ << '\n';
        }
    }   
    os_ <<
        "|===\n" <<
        "\n";
    endSection();
}

void
AdocWriter::
writeNestedTypes(
    llvm::StringRef sectionName,
    std::vector<Reference> const& list,
    AccessSpecifier access)
{
    if(list.empty())
        return;
    beginSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(auto const& ref : list)
    {
        auto& I = corpus_.get<TypedefInfo>(ref.id);
#if 0
        if(it->Access != access)
            continue;
#endif
        os_ <<
            "|`" << I.Name << "`\n" <<
            "|";
        writeBrief(I.javadoc, false);
        os_ << '\n';
    }   
    os_ <<
        "|===\n" <<
        "\n";
    endSection();
}

void
AdocWriter::
writeDataMembers(
    llvm::StringRef sectionName,
    llvm::SmallVectorImpl<MemberTypeInfo> const& list,
    AccessSpecifier access)
{
    if(list.empty())
        return;

    auto it = list.begin();
    for(;;)
    {
        if(it == list.end())
            return;
        if(it->Access == access)
            break;
        ++it;
    }
    beginSection(sectionName);
    os_ <<
        "\n"
        "[,cols=2]\n" <<
        "|===\n" <<
        "|Name |Description\n" <<
        "\n";
    for(;it != list.end(); ++it)
    {
        if(it->Access != access)
            continue;
        os_ <<
            "|`" << it->Name << "`\n" <<
            "|";
        writeBrief(it->javadoc, false);
        os_ << '\n';
    }   
    os_ <<
        "|===\n" <<
        "\n";
    endSection();
}

//------------------------------------------------

void
AdocWriter::
writeBrief(
    llvm::Optional<Javadoc> const& javadoc,
    bool withNewline)
{
    if(! javadoc.has_value())
        return;
    auto const node = javadoc->getBrief();
    if(! node)
        return;
    if(node->empty())
        return;
    if(withNewline)
        os_ << '\n';
    writeNode(*node);
}

void
AdocWriter::
writeDescription(
    llvm::Optional<Javadoc> const& javadoc)
{
    if(! javadoc.has_value())
        return;

    //os_ << '\n';
    beginSection("Description");
    os_ << '\n';
    writeNodes(javadoc->getBlocks());
    endSection();
}

void
AdocWriter::
writeLocation(
    SymbolInfo const& I)
{
    Location const* loc = nullptr;
    if(I.DefLoc.has_value())
        loc = &*I.DefLoc;
    else if(! I.Loc.empty())
        loc = &I.Loc[0];

    std::string url;
    //--------------------------------------------
    llvm::raw_string_ostream os(url);

    // relative href
#if 1
    os << "link:" << loc->Filename;
#else
    os << "file:///" << loc->Filename;
#endif
    //--------------------------------------------

    switch(I.IT)
    {
    default:
    case InfoType::IT_function:
        os_ <<
            "\n"
            "Declared in " << url <<
            "[" << loc->Filename << "]" <<
            "\n";
        break;
    case InfoType::IT_record:
        os_ <<
            "\n"
            "`#include <" << url <<
            "[" << loc->Filename << "]" <<
            ">`\n";
        break;
    }
}

void
AdocWriter::
writeFunctionDeclaration(
    FunctionInfo const& I)
{
    os_ << typeName(I.ReturnType) << ' ' << I.Name;
    if(I.Params.empty())
    {
        os_ << "()";
        return;
    }
    os_ << "( " << formalParam(I.Params[0]);
    for(std::size_t i = 1; i < I.Params.size(); ++i)
        os_ << ", " << formalParam(I.Params[i]);
    os_ << " )";
}

//------------------------------------------------

void
AdocWriter::
writeNode(
    Javadoc::Node const& node)
{
    switch(node.kind)
    {
    case Javadoc::Kind::text:
        writeNode(static_cast<Javadoc::Text const&>(node));
        return;
    case Javadoc::Kind::styled:
        writeNode(static_cast<Javadoc::StyledText const&>(node));
        return;
#if 0
    case Javadoc::Node::block:
        writeNode(static_cast<Javadoc::Block const&>(node));
        return;
#endif
    case Javadoc::Kind::brief:
    case Javadoc::Kind::paragraph:
        writeNode(static_cast<Javadoc::Paragraph const&>(node));
        return;
    case Javadoc::Kind::admonition:
        writeNode(static_cast<Javadoc::Admonition const&>(node));
        return;
    case Javadoc::Kind::code:
        writeNode(static_cast<Javadoc::Code const&>(node));
        return;
    case Javadoc::Kind::param:
        writeNode(static_cast<Javadoc::Param const&>(node));
        return;
    case Javadoc::Kind::tparam:
        writeNode(static_cast<Javadoc::TParam const&>(node));
        return;
    case Javadoc::Kind::returns:
        writeNode(static_cast<Javadoc::Returns const&>(node));
        return;
    default:
        llvm_unreachable("unknown kind");
    }
}

void
AdocWriter::
writeNode(
    Javadoc::Block const&)
{
    // shouldn't get here
    Assert(false);
}

void
AdocWriter::
writeNode(
    Javadoc::Text const& node)
{
    // Text nodes must be left aligned or
    // else they can be rendered up as code.
    os_ << llvm::StringRef(node.string).ltrim() << '\n';
}

void
AdocWriter::
writeNode(
    Javadoc::StyledText const& node)
{
    switch(node.style)
    {
    case Javadoc::Style::bold:
        os_ << '*' << node.string << "*\n";
        break;
    case Javadoc::Style::mono:
        os_ << '`' << node.string << "`\n";
        break;
    case Javadoc::Style::italic:
        os_ << '_' << node.string << "_\n";
        break;
    default:
        os_ << node.string << '\n';
        break;
    }
}

void
AdocWriter::
writeNode(
    Javadoc::Paragraph const& node)
{
    writeNodes(node.children);
}

void
AdocWriter::
writeNode(
    Javadoc::Admonition const& node)
{
    writeNodes(node.children);
}

void
AdocWriter::
writeNode(
    Javadoc::Code const& node)
{
    os_ <<
        "[,cpp]\n"
        "----\n";
    AnyList<Javadoc::Text> const& list(node.children);
    if(! list.empty())
    {
        // measure the left margin
        std::size_t n = std::size_t(-1);
        for(auto& text : list)
        {
            auto const space = text.string.size() -
                llvm::StringRef(text.string).ltrim().size();
            if( n > space)
                n = space;
        }
        // now write left-aligned
        for(auto& text : list)
        {
            llvm::StringRef string(text.string);
            string = string.ltrim(n);
            os_ << string << "\n";
        }
    }
    os_ <<
        "----\n";
}

void
AdocWriter::
writeNode(
    Javadoc::Param const& node)
{
}

void
AdocWriter::
writeNode(
    Javadoc::TParam const& node)
{
}

void
AdocWriter::
writeNode(
    Javadoc::Returns const& node)
{
}

//------------------------------------------------

auto
AdocWriter::
typeName(
    TypeInfo const& t) ->
        TypeName
{
    return TypeName{ t, corpus_, *this };
}

//------------------------------------------------

void
AdocWriter::
beginSection(
    Info const& I)
{
    //Assert(validAdocSectionID(names_.get(I.id)));

    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    os_ <<
        "\n" <<
        "[\"#" << names_.get(I.id) << "\"]\n" <<
        sect_.markup << ' ' << I.Name << "\n";
}

void
AdocWriter::
beginSection(
    Info const& P,
    OverloadInfo const& I)
{
    sect_.level++;
    if(sect_.level <= 6)
        sect_.markup.push_back('=');
    std::string temp;
    os_ <<
        "\n" <<
        "[\"#" << names_.getOverload(P, I.Name, '-', temp) << "\"]\n" <<
        sect_.markup << ' ' << I.Name << "\n";
}
void
AdocWriter::
beginSection(
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
AdocWriter::
endSection()
{
    Assert(sect_.level > 0);
    if(sect_.level <= 6)
        sect_.markup.pop_back();
    sect_.level--;
}

//------------------------------------------------

llvm::StringRef
AdocWriter::
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

} // adoc
} // mrdox
} // clang
