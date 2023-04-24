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

#ifndef MRDOX_LIB_ADOC_ADOCWRITER_HPP
#define MRDOX_LIB_ADOC_ADOCWRITER_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <string>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocWriter
    : public Corpus::Visitor
{
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

public:
    AdocWriter(
        llvm::raw_ostream& os,
        llvm::raw_fd_ostream* fd_os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    llvm::Error build();

    struct FormalParam;
    struct TypeName;

    void writeFormalParam(FormalParam const& p, llvm::raw_ostream& os);
    void writeTypeName(TypeName const& tn, llvm::raw_ostream& os);

protected:
    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;

protected:
    void writeRecord(RecordInfo const& I);
    void writeFunction(FunctionInfo const& I);
    void writeTypedef(TypedefInfo const& I);
    void writeEnum(EnumInfo const& I);

    void writeBase(BaseRecordInfo const& I);
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

    void openSection(llvm::StringRef name);
    void closeSection();

    static llvm::StringRef toString(TagTypeKind k) noexcept;
};

} // adoc
} // mrdox
} // clang

#endif
