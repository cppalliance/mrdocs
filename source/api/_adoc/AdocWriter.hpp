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

#include "SafeNames.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <string>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocWriter
{
    SafeNames const& names_;

protected:
    struct Section
    {
        int level = 0;
        std::string markup;
    };

    llvm::raw_ostream& os_;
    llvm::raw_fd_ostream* fd_os_;
    Corpus const& corpus_;
    Reporter& R_;
    Section sect_;
    std::string temp_;

    friend class TypeName;

public:
    virtual ~AdocWriter() = default;

    AdocWriter(
        llvm::raw_ostream& os,
        llvm::raw_fd_ostream* fd_os,
        SafeNames const& names,
        Corpus const& corpus,
        Reporter& R) noexcept;

    struct FormalParam;
    struct TypeName;

protected:
    void write(NamespaceInfo const& I);
    void write(RecordInfo const& I);
    void write(FunctionInfo const& I);
    void write(TypedefInfo const& I);
    void write(EnumInfo const& I);

    virtual llvm::StringRef linkFor(Info const&);

    void writeBase(
        BaseRecordInfo const& I);
    void writeFunctionOverloads(
        llvm::StringRef sectionName,
        OverloadsSet const& set);
    void writeNestedTypes(
        llvm::StringRef sectionName,
        std::vector<TypedefInfo> const& list,
        AccessSpecifier access);
    void writeDataMembers(
        llvm::StringRef sectionName,
        llvm::SmallVectorImpl<MemberTypeInfo> const& list,
        AccessSpecifier access);

    void writeBrief(
        llvm::Optional<Javadoc> const& javadoc,
        bool withNewline = true);
    void writeDescription(
        llvm::Optional<Javadoc> const& javadoc);
    void writeLocation(SymbolInfo const& I);

    template<class T>
    void writeNodes(List<T> const& list);
    void writeNode(Javadoc::Node const& node);
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
    void beginSection(llvm::StringRef name);
    void endSection();

    static llvm::StringRef toString(TagTypeKind k) noexcept;
};

} // adoc
} // mrdox
} // clang

#endif
