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

#ifndef MRDOX_SOURCE_XML_XML_HPP
#define MRDOX_SOURCE_XML_XML_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/meta/Javadoc.hpp>

namespace clang {
namespace mrdox {
namespace xml {

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

    llvm::Error
    buildSinglePage(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R,
        llvm::raw_fd_ostream* fd_os) const override;
};

//------------------------------------------------

/** A writer which outputs XML.
*/
class XMLGenerator::Writer
    : public Corpus::Visitor
{
    std::string indentString_;
    llvm::raw_ostream& os_;
    llvm::raw_fd_ostream* fd_os_;
    Corpus const& corpus_;
    Reporter& R_;

public:
    /** Describes an item in the list of all symbols.
    */
    struct AllSymbol
    {
        /** The fully qualified name of this symbol.
        */
        std::string fqName;

        /** A string representing the symbol type.
        */
        llvm::StringRef symbolType;

        /** The ID of this symbol.
        */
        SymbolID id;

        /** Constructor.
        */
        AllSymbol(Info const& I);
    };

    struct Attr;

    struct Attrs
    {
        std::initializer_list<Attr> init_;
        Attrs() = default;
        Attrs(std::initializer_list<Attr> init);
        friend llvm::raw_ostream& operator<<(
            llvm::raw_ostream& os, Attrs const& attrs);
    };

    struct maybe_indent_type;
    maybe_indent_type
    maybe_indent() noexcept;

    Writer(
        llvm::raw_ostream& os,
        llvm::raw_fd_ostream* fd_os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    llvm::Error build();

private:
    void writeAllSymbols();

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;

    void writeInfo(Info const& I);
    void writeSymbol(SymbolInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeBaseRecord(BaseRecordInfo const& I);
    void writeParam(FieldTypeInfo const& I);
    void writeTemplateParam(TemplateParamInfo const& I);
    void writeMemberType(MemberTypeInfo const& I);
    void writeReturnType(TypeInfo const& I);
    void writeJavadoc(llvm::Optional<Javadoc> const& javadoc);

    template<class T>
    void writeNodes(List<T> const& list);
    void writeNode(Javadoc::Node const& node);
    void writeBrief(Javadoc::Paragraph const& node);
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

    llvm::raw_ostream& indent();
    void adjustNesting(int levels);

    static std::string toString(SymbolID const& id);
    static llvm::StringRef toString(InfoType) noexcept;
    static llvm::StringRef toString(Javadoc::Style style) noexcept;
};

//------------------------------------------------

} // xml
} // mrdox
} // clang

#endif
