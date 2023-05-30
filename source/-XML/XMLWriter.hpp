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
#include "Support/YamlFwd.hpp"
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
    template<class T>
    friend struct llvm::yaml::MappingTraits;

protected:
    XMLTags tags_;
    llvm::raw_ostream& os_;
    Corpus const& corpus_;
    Reporter& R_;

    struct GenKey;
    struct XmlKey;
    struct Options
    {
        bool index = false;
        bool prolog = true;
        bool safe_names = false;
    };
    Options options_;

public:
    XMLWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    Err build();

private:
    void writeIndex();

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
    bool visit(VarInfo const&) override;

    bool visit(MemberEnum const&, Access) override;
    bool visit(MemberFunction const&, Access) override;
    bool visit(MemberRecord const&, Access) override;
    bool visit(MemberType const&, Access) override;
    bool visit(DataMember const&, Access) override;
    bool visit(StaticDataMember const&, Access) override;

    bool writeEnum(EnumInfo const&, Access const* access = nullptr);
    bool writeFunction(FunctionInfo const&, Access const* access = nullptr);
    bool writeRecord(RecordInfo const&, Access const* access = nullptr);
    bool writeTypedef(TypedefInfo const&, Access const* access = nullptr);
    bool writeField(FieldInfo const&, Access const* access = nullptr);
    bool writeVar(VarInfo const&, Access const* access = nullptr);

    void writeInfo(Info const&);
    void writeSymbol(SymbolInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeJavadoc(std::unique_ptr<Javadoc> const& javadoc);
    void openTemplate(const std::unique_ptr<TemplateInfo>& I);
    void closeTemplate(const std::unique_ptr<TemplateInfo>& I);

    template<class T>
    void writeNodes(AnyList<T> const& list);
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
