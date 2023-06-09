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

#include "Support/SafeNames.hpp"
#include "Support/YamlFwd.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Error.hpp>
#include <string>

namespace clang {
namespace mrdox {
namespace adoc {

inline
llvm::StringRef
toString(Access access) noexcept
{
    switch(access)
    {
    case Access::Public: return "public";
    case Access::Protected: return "protected";
    case Access::Private: return "private";
    default:
        llvm_unreachable("unknown access");
    }
}

class AdocWriter
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

protected:
    struct Options
    {
        bool safe_names = false;
        std::string template_dir;
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
    Section sect_;
    std::string temp_;

    friend struct TypeName;
public:
    virtual ~AdocWriter() = default;

    AdocWriter(
        llvm::raw_ostream& os,
        SafeNames const& names,
        Corpus const& corpus) noexcept;

    Error init();

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

    template<class T>
    void writeTrancheList(
        llvm::StringRef sectionName,
        std::span<T const> list);

    void writeBase(BaseInfo const& I);
    void writeNestedTypes(
        llvm::StringRef sectionName,
        std::vector<Reference> const& list,
        AccessSpecifier access);

    void writeFunctionDeclaration(
        FunctionInfo const& I);

    void writeBrief(
        std::unique_ptr<Javadoc> const& javadoc,
        bool withNewline = true);
    void writeDescription(
        std::unique_ptr<Javadoc> const& javadoc);
    void writeLocation(
        Info const& I,
        SymbolInfo const& S);

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

    FormalParam formalParam(Param const& ft);
    TypeName typeName(TypeInfo const& ti);

    void beginSection(Info const& I);
    void beginSection(Info const& P, OverloadInfo const& F);
    void beginSection(llvm::StringRef name);
    void endSection();

    static llvm::StringRef recordKeyToString(RecordKeyKind k) noexcept;

    //--------------------------------------------

    void declareRecord(RecordInfo const& I);
    void declareFunction(FunctionInfo const& I);
};

} // adoc
} // mrdox
} // clang

#endif
