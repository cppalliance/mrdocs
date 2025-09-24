//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_XML_XMLWRITER_HPP
#define MRDOCS_LIB_GEN_XML_XMLWRITER_HPP

#include <lib/Gen/xml/XMLTags.hpp>
#include <lib/Support/YamlFwd.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string>

namespace clang::mrdocs::xml {

class jit_indenter;

/** A writer which outputs XML.
*/
class XMLWriter
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

protected:
    XMLTags tags_;
    llvm::raw_ostream& os_;
    Corpus const& corpus_;

public:
    XMLWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus) noexcept;

    Expected<void>
    build();

    void writeIndex();

    template<class T>
    void operator()(T const&);


    // ---------------
    // Info types

#define INFO(Type) void write##Type(Type##Info const&);
#include <mrdocs/Metadata/Info/InfoNodes.inc>

    void writeSourceInfo(SourceInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeJavadoc(std::optional<Javadoc> const& javadoc);
    void writeFriend(FriendInfo const& I);
    void openTemplate(const std::optional<TemplateInfo>& I);
    void closeTemplate(const std::optional<TemplateInfo>& I);

    // ---------------
    // Javadoc types

    void writeAdmonition(doc::Admonition const& node);
    void writeBrief(doc::Paragraph const& node);
    void writeCode(doc::Code const& node);
    void writeHeading(doc::Heading const& node);
    void writeLink(doc::Link const& node);
    void writeListItem(doc::ListItem const& node);
    void writeUnorderedList(doc::UnorderedList const& node);
    void writeParagraph(doc::Paragraph const& node, llvm::StringRef tag = "");
    void writeJParam(doc::Param const& node);
    void writeReturns(doc::Returns const& node);
    void writeStyledText(doc::Styled const& node);
    void writeText(doc::Text const& node);
    void writeTParam(doc::TParam const& node);
    void writeReference(doc::Reference const& node);
    void writeCopied(doc::CopyDetails const& node);
    void writeThrows(doc::Throws const& node);
    void writeSee(doc::See const& node, llvm::StringRef tag = "");
    void writePrecondition(doc::Precondition const& node);
    void writePostcondition(doc::Postcondition const& node);

    void writeNode(doc::Node const& node);

    template <class T>
    void
    writeNodes(std::vector<Polymorphic<T>> const& list)
    {
        for (auto const& node: list)
        {
            writeNode(*node);
        }
    }

    template <std::derived_from<doc::Node> T>
    void writeNodes(std::vector<T> const& list)
    {
        for (auto const& node: list)
        {
            writeNode(node);
        }
    }
};

} // clang::mrdocs::xml

#endif // MRDOCS_LIB_GEN_XML_XMLWRITER_HPP
