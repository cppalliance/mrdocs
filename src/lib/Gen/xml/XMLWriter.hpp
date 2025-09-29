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

namespace mrdocs::xml {

class jit_indenter;

/** A writer that outputs XML.
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

    template <std::derived_from<Symbol> SymbolTy>
    void
    operator()(SymbolTy const& I);


    // ---------------
    // Info types

#define INFO(Type) void write##Type(Type##Symbol const&);
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

    void writeSourceInfo(SourceInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeDocComment(Optional<DocComment> const& doc);
    void writeFriend(FriendInfo const& I);
    void openTemplate(Optional<TemplateInfo> const& I);
    void closeTemplate(Optional<TemplateInfo> const& I);

    // ---------------
    // DocComment types

#define INFO(Type) void write##Type(doc::Type##Block const&);
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

    void
    writeBlock(doc::Block const& node);

    template <std::derived_from<doc::Block> T>
    void
    writeBlocks(std::vector<T> const& list)
    {
        for (auto const& node: list)
        {
            writeBlock(node);
        }
    }

    void
    writeBlocks(std::vector<Polymorphic<doc::Block>> const& list)
    {
        for (auto const& node: list)
        {
            writeBlock(*node);
        }
    }

#define INFO(Type) void write##Type(doc::Type##Inline const&);
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

    void
    writeInline(doc::Inline const& node);

    template <std::derived_from<doc::Inline> T>
    void
    writeInlines(std::vector<T> const& list)
    {
        for (auto const& node: list)
        {
            writeInline(node);
        }
    }

    void
    writeInlines(std::vector<Polymorphic<doc::Inline>> const& list)
    {
        for (auto const& node: list)
        {
            writeInline(*node);
        }
    }

    void
    writeInlineContainer(
        doc::InlineContainer const& node,
        std::string_view tag);

    void
    writeListItem(doc::ListItem const& node);

//    void
//    writeNode(doc::Node const& node);

    template <class T>
    void
    writeNodes(std::vector<Polymorphic<T>> const& list)
    {
        for (auto const& node: list)
        {
            writeNode(*node);
        }
    }

//    template <std::derived_from<doc::Node> T>
//    void writeNodes(std::vector<T> const& list)
//    {
//        for (auto const& node: list)
//        {
//            writeNode(node);
//        }
//    }
};

} // mrdocs::xml

#endif // MRDOCS_LIB_GEN_XML_XMLWRITER_HPP
