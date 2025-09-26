//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TagfileWriter.hpp"
#include <lib/ConfigImpl.hpp>
#include <lib/Gen/hbs/VisitorHelpers.hpp>
#include <lib/Gen/xml/CXXTags.hpp>
#include <lib/Support/RawOstream.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>

namespace clang::mrdocs {

//------------------------------------------------
//
// TagfileWriter
//
//------------------------------------------------

TagfileWriter::
TagfileWriter(
    hbs::HandlebarsCorpus const& corpus,
    llvm::raw_ostream& os,
    std::string_view const defaultFilename
    ) noexcept
    : corpus_(corpus)
    , os_(os)
    , tags_(os)
    , defaultFilename_(defaultFilename)
{}

Expected<TagfileWriter>
TagfileWriter::
create(
    hbs::HandlebarsCorpus const& corpus,
    llvm::raw_ostream& os,
    std::string_view defaultFilename)
{
    return TagfileWriter(corpus, os, defaultFilename);
}

void
TagfileWriter::
build()
{
    initialize();
    operator()(corpus_->globalNamespace());
    finalize();
}

void
TagfileWriter::
initialize()
{
    os_ << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    os_ << "<tagfile>\n";
}

void
TagfileWriter::
finalize()
{
    os_ << "</tagfile>\n";
}


template<class T>
void
TagfileWriter::
operator()(T const& I)
{
    if constexpr (std::derived_from<T, Info>)
    {
        if (!hbs::shouldGenerate(I, corpus_.getCorpus().config))
        {
            return;
        }
    }

    if constexpr (T::isNamespace())
    {
        // Namespaces are compound elements with members
        writeNamespace(I);
    }
    else if (!T::isFunction())
    {
        // Functions are described as namespace members in the
        // scoped they belong to.
        // Everything else is described as a compound element of
        // type "class" because it's the type doxygen supports.
        writeClassLike(I);
    }
}

#define INFO(Type) template void TagfileWriter::operator()<Type##Info>(Type##Info const&);
#include <mrdocs/Metadata/Info/InfoNodes.inc>

void
TagfileWriter::
writeNamespace(
    NamespaceInfo const& I)
{
    // Check if this namespace contains only other namespaces
    bool onlyNamespaces = true;
    corpus_->traverse(I, [&](Info const& U)
    {
        if (!hbs::shouldGenerate(U, corpus_.getCorpus().config))
        {
            return;
        }

        if (U.Kind != InfoKind::Namespace)
        {
            onlyNamespaces = false;
        }
    });

    // Write the compound element for this namespace
    if (!onlyNamespaces)
    {
        tags_.open("compound", {
            { "kind", "namespace" }
        });

        tags_.write("name", corpus_->qualifiedName(I));
        tags_.write("filename", generateFilename(I));

        // Write the class-like members of this namespace
        corpus_->traverse(I, [this]<typename U>(U const& J)
        {
            if (!hbs::shouldGenerate(J, corpus_.getCorpus().config))
            {
                return;
            }

            if (!U::isNamespace() && !U::isFunction())
            {
                tags_.write(
                    "class",
                    corpus_->qualifiedName(J),
                    {{"kind", "class"}});
            }
        });

        // Write the function-like members of this namespace
        corpus_->traverse(I, [this]<typename U>(U const& J)
        {
            if constexpr (U::isFunction())
            {
                writeFunctionMember(J);
            }
        });

        tags_.close("compound");
    }

    // Write compound elements for the members of this namespace
    corpus_->traverse(I, [this]<typename U>(U const& J)
        {
            this->operator()(J);
        });
}

template<class T>
void
TagfileWriter::
writeClassLike(
    T const& I
)
{
    tags_.open("compound", {
        { "kind", "class" }
    });
    tags_.write("name", corpus_->qualifiedName(I));
    tags_.write("filename", generateFilename(I));
    if constexpr (T::isRecord())
    {
        // Write the function-like members of this record
        corpus_->traverse(I, [this]<typename U>(U const& J)
        {
            if constexpr (U::isFunction())
            {
                writeFunctionMember(J);
            }
        });
    }
    tags_.close("compound");
}

void
TagfileWriter::
writeFunctionMember(FunctionInfo const& I)
{
    tags_.open("member", {{"kind", "function"}});
    MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
    tags_.write("type", toString(*I.ReturnType));
    tags_.write("name", I.Name);
    auto [anchorFile, anchor] = generateFileAndAnchor(I);
    tags_.write("anchorfile", anchorFile);
    tags_.write("anchor", anchor);
    std::string arglist = "(";
    for(auto const& J : I.Params)
    {
        MRDOCS_ASSERT(!J.Type.valueless_after_move());
        arglist += toString(*J.Type);
        if (J.Name)
        {
            arglist += " ";
            arglist += *J.Name;
        }
        arglist += ", ";
    }
    if (arglist.size() > 2) {
        arglist.resize(arglist.size() - 2);
    }
    arglist += ")";
    tags_.write("arglist", arglist);
    tags_.close("member");
}


template<class T>
std::string
TagfileWriter::
generateFilename(T const& I)
{
    std::string url = corpus_.getURL(I);
    // In single page mode, getURL always returns an
    // anchor.
    if (!corpus_->config->multipage) {
        // filename is #<anchor>
        if (!url.starts_with('#'))
        {
            return defaultFilename_ + '#' + url;
        }
        return defaultFilename_ + url;
    }
    // In multipage mode, getURL returns a file path
    // starting with a slash, and it may contain an anchor
    if (url.starts_with('/'))
    {
        url.erase(0, 1);
    }
    return url;
}

template<class T>
std::pair<std::string, std::string>
TagfileWriter::
generateFileAndAnchor(T const& I)
{
    std::string url = corpus_.getURL(I);
    // Make relative to output dir
    if (url.starts_with('/'))
    {
        url.erase(0, 1);
    }
    // In single page mode, getURL always returns an
    // anchor. So the filename is the default and the
    // anchor is the URL.
    if (!corpus_->config->multipage) {
        if (url.starts_with('#'))
        {
            url.erase(0, 1);
        }
        return {defaultFilename_, url};
    }
    // In multipage mode, getURL returns a file path
    // starting with a slash, and it may contain an anchor
    auto const pos = url.find('#');
    if (pos == std::string::npos)
    {
        return {url, ""};
    }
    return {url.substr(0, pos), url.substr(pos + 1)};
}

} // clang::mrdocs