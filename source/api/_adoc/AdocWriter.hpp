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

#ifndef MRDOX_API_ADOC_ADOCWRITER_HPP
#define MRDOX_API_ADOC_ADOCWRITER_HPP

#include "Support/SafeNames.hpp"
#include "Support/YamlFwd.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <string>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocWriter
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

protected:
    struct Options
    {
        bool safe_names = false;
    };

    struct Key;
    struct GenKey;
    Options options_;
    SafeNames const& names_;

    struct Section
    {
        int level = 0;
        std::string markup;
    };

    llvm::raw_ostream& os_;
    Corpus const& corpus_;
    Reporter& R_;
    Section sect_;
    std::string temp_;

    friend class TypeName;

public:
    virtual ~AdocWriter() = default;

    AdocWriter(
        llvm::raw_ostream& os,
        SafeNames const& names,
        Corpus const& corpus,
        Reporter& R) noexcept;

    llvm::Error init();

    struct FormalParam;
    struct TypeName;

protected:
    void write(NamespaceInfo const& I);
    void write(RecordInfo const& I);
    void write(FunctionInfo const& I);
    void write(TypedefInfo const& I);
    void write(EnumInfo const& I);
    void write(VarInfo const& I);

    virtual llvm::StringRef linkFor(Info const&);
    virtual llvm::StringRef linkFor(Info const&, OverloadInfo const&);

    void writeLinkFor(Info const&);
    void writeLinkFor(OverloadInfo const&);

    void writeBase(BaseInfo const& I);
    void writeFunctionOverloads(
        llvm::StringRef sectionName,
        OverloadsSet const& set);
    void writeNestedTypes(
        llvm::StringRef sectionName,
        std::vector<Reference> const& list,
        AccessSpecifier access);
    void writeDataMembers(
        llvm::StringRef sectionName,
        llvm::SmallVectorImpl<MemberTypeInfo> const& list,
        AccessSpecifier access);

    void writeFunctionDeclaration(
        FunctionInfo const& I);

    void writeBrief(
        llvm::Optional<Javadoc> const& javadoc,
        bool withNewline = true);
    void writeDescription(
        llvm::Optional<Javadoc> const& javadoc);
    void writeLocation(SymbolInfo const& I);

    template<class T>
    void writeNodes(AnyList<T> const& list)
    {
        if(list.empty())
            return;
        for(Javadoc::Node const& node : list)
            writeNode(node);
    }

    void writeNode(Javadoc::Node const& node);
    void writeNode(Javadoc::Block const& node);
    void writeNode(Javadoc::Text const& node);
    void writeNode(Javadoc::StyledText const& node);
    void writeNode(Javadoc::Paragraph const& node);
    void writeNode(Javadoc::Admonition const& node);
    void writeNode(Javadoc::Code const& node);
    void writeNode(Javadoc::Param const& node);
    void writeNode(Javadoc::TParam const& node);
    void writeNode(Javadoc::Returns const& node);

    FormalParam formalParam(FieldTypeInfo const& ft);
    TypeName typeName(TypeInfo const& ti);

    void beginSection(Info const& I);
    void beginSection(Info const& P, OverloadInfo const& F);
    void beginSection(llvm::StringRef name);
    void endSection();

    static llvm::StringRef toString(TagTypeKind k) noexcept;
};

} // adoc
} // mrdox
} // clang

#endif
