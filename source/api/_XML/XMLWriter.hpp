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

#ifndef MRDOX_API_XML_XMLWRITER_HPP
#define MRDOX_API_XML_XMLWRITER_HPP

#include "XMLTags.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <string>

namespace clang {
namespace mrdox {
namespace xml {

class jit_indenter;

/** A writer which outputs XML.
*/
class XMLWriter
    : public Corpus::Visitor
{
protected:
    XMLTags tags_;
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

    XMLWriter(
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
    bool visit(VariableInfo const&) override;

    void writeInfo(Info const&);
    void writeSymbol(SymbolInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeBaseRecord(BaseRecordInfo const& I);
    void writeTemplateParam(TemplateParamInfo const& I);
    void writeMemberType(MemberTypeInfo const& I);
    void writeStorageClass(jit_indenter&, StorageClass SC);
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
    void writeJParam(Javadoc::Param const& node);
    void writeTParam(Javadoc::TParam const& node);
};

} // xml
} // mrdox
} // clang

#endif
