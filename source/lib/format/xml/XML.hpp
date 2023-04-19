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

#ifndef MRDOX_SOURCE_FORMAT_XML_HPP
#define MRDOX_SOURCE_FORMAT_XML_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/format/Generator.hpp>
#include <mrdox/format/RecursiveWriter.hpp>

namespace clang {
namespace mrdox {

//------------------------------------------------

struct XMLGenerator : Generator
{
    class Writer;

    llvm::StringRef
    name() const noexcept override
    {
        return "XML";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "xml";
    }

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

/** A writer which outputs XML.
*/
class XMLGenerator::Writer
    : public RecursiveWriter
{
public:
    struct Attr;

    struct Attrs
    {
        std::initializer_list<Attr> init_;
        Attrs() = default;
        Attrs(std::initializer_list<Attr> init);
        friend llvm::raw_ostream& operator<<(
            llvm::raw_ostream& os, Attrs const& attrs);
    };

    Writer(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept;

    void write();

private:
    void writeAllSymbols();

    void visitNamespace(NamespaceInfo const&) override;
    void visitRecord(RecordInfo const&) override;
    void visitFunction(FunctionInfo const&) override;
    void visitTypedef(TypedefInfo const& I) override;
    void visitEnum(EnumInfo const& I) override;

    void writeInfo(Info const& I);
    void writeSymbol(SymbolInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeBaseRecord(BaseRecordInfo const& I);
    void writeParam(FieldTypeInfo const& I);
    void writeTemplateParam(TemplateParamInfo const& I);
    void writeMemberType(MemberTypeInfo const& I);
    void writeReturnType(TypeInfo const& I);
    void writeJavadoc(Javadoc const& jd);

    template<class T>
    void writeNodes(List<T> const& list);
    void writeNode(Javadoc::Node const& node);
    void writeBrief(Javadoc::Paragraph const* node);
    void writeText(Javadoc::Text const& node);
    void writeStyledText(Javadoc::StyledText const& node);
    void writeParagraph(Javadoc::Paragraph const& node, llvm::StringRef tag = "");
    void writeAdmonition(Javadoc::Admonition const& node);
    void writeCode(Javadoc::Code const& node);
    void writeReturns(Javadoc::Returns const& node);
    void writeParam(Javadoc::Param const& node);
    void writeTParam(Javadoc::TParam const& node);

    void openTag(llvm::StringRef, Attrs = {});
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef tag,
        llvm::StringRef value = {}, Attrs = {});

    static std::string toString(SymbolID const& id);
    static llvm::StringRef toString(InfoType) noexcept;
    static llvm::StringRef toString(Javadoc::Style style) noexcept;
};

//------------------------------------------------

} // mrdox
} // clang

#endif
