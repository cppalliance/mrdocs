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

#ifndef MRDOX_SOURCE_ASCIIDOC_HPP
#define MRDOX_SOURCE_ASCIIDOC_HPP

#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/format/FlatWriter.hpp>
#include <mrdox/format/Generator.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

struct OverloadSet;

class AsciidocGenerator
    : public Generator
{
public:
    class Writer;

    llvm::StringRef
    name() const noexcept override
    {
        return "Asciidoc";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "adoc";
    }

    bool
    build(
        llvm::StringRef rootPath,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    buildString(
        std::string& dest,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;
};

//------------------------------------------------

class AsciidocGenerator::Writer
    : public FlatWriter
{
    struct Section
    {
        int level = 0;
        std::string markup;
    };

    Section sect_;

public:
    Writer(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept;

    void beginFile() override;
    void endFile() override;

    struct FormalParam;
    struct TypeName;

    void writeFormalParam(FormalParam const& p, llvm::raw_ostream& os);
    void writeTypeName(TypeName const& tn, llvm::raw_ostream& os);

protected:
    void writeRecord(RecordInfo const& I) override;
    void writeFunction(FunctionInfo const& I) override;
    void writeEnum(EnumInfo const& I) override;
    void writeTypedef(TypedefInfo const& I) override;

    void writeLocation(Location const&);
    void writeBase(BaseRecordInfo const& I);
    void writeOverloadSet(
        llvm::StringRef sectionName,
        std::vector<OverloadSet> const& list);
    void writeMemberTypes(
        llvm::StringRef sectionName,
        llvm::SmallVectorImpl<MemberTypeInfo> const& list,
        AccessSpecifier access);

    void writeBrief(Javadoc::Paragraph const* node);
    void writeDescription(List<Javadoc::Block> const& list);

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

    void openTitle(llvm::StringRef name);
    void openSection(llvm::StringRef name);
    void closeSection();

    static Location const& getLocation(SymbolInfo const& I);
    static llvm::StringRef toString(TagTypeKind k) noexcept;
};

} // mrdox
} // clang

#endif
