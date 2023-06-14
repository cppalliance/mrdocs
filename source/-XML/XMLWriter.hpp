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

#ifndef MRDOX_TOOL_XML_XMLWRITER_HPP
#define MRDOX_TOOL_XML_XMLWRITER_HPP

#include "XMLTags.hpp"
#include "Support/YamlFwd.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Error.hpp>
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
        Corpus const& corpus) noexcept;

    Error build();

private:
    void writeIndex();

    bool visit(NamespaceInfo const&) override;
    bool visit(RecordInfo const&) override;
    bool visit(FunctionInfo const&) override;
    bool visit(TypedefInfo const&) override;
    bool visit(EnumInfo const&) override;
    bool visit(VariableInfo const&) override;
    bool visit(SpecializationInfo const&) override;
    bool visit(FieldInfo const&) override;

    bool writeEnum(EnumInfo const&);
    bool writeFunction(FunctionInfo const&);
    bool writeRecord(RecordInfo const&);
    bool writeTypedef(TypedefInfo const&);
    bool writeField(FieldInfo const&);
    bool writeVar(VariableInfo const&);
    bool writeSpecialization(const SpecializationInfo&);

    void writeSourceInfo(SourceInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeJavadoc(std::unique_ptr<Javadoc> const& javadoc);
    void openTemplate(const std::unique_ptr<TemplateInfo>& I);
    void closeTemplate(const std::unique_ptr<TemplateInfo>& I);

    template<class T>
    void writeNodes(doc::List<T> const& list);
    void writeNode(doc::Node const& node);
    void writeBrief(doc::Paragraph const& node);
    void writeText(doc::Text const& node);
    void writeStyledText(doc::StyledText const& node);
    void writeParagraph(doc::Paragraph const& node, llvm::StringRef tag = "");
    void writeAdmonition(doc::Admonition const& node);
    void writeCode(doc::Code const& node);
    void writeReturns(doc::Returns const& node);
    void writeJParam(doc::Param const& node);
    void writeTParam(doc::TParam const& node);
};

} // xml
} // mrdox
} // clang

#endif
