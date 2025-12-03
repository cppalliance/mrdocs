//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP
#define MRDOCS_LIB_GEN_HBS_HANDLEBARSGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Gen/hbs/HandlebarsCorpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <utility>

namespace mrdocs {

class OutputRef;

namespace hbs {

class HandlebarsGenerator
    : public Generator
{
public:
    struct StylesheetRef
    {
        /** Absolute path to the source stylesheet on disk (empty for remote). */
        std::string sourcePath;
        /** Href or output-relative path written to the HTML. */
        std::string outputRelative;
        /** True when the stylesheet is remote and should not be copied. */
        bool external = false;
    };

    struct StylesData
    {
        std::vector<StylesheetRef> stylesheets;
        std::vector<std::string> inlineStyles;
        std::vector<std::string> inlineScripts;
        bool hasDefaultStyles = false;
    };

private:
    Expected<StylesData> prepareStylesheets(Config const& config) const;

public:
    Expected<void>
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    Expected<void>
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;

    /** Build a tagfile for the corpus.
    */
    Expected<void>
    buildTagfile(
        std::ostream& os,
        Corpus const& corpus) const;

    /** Build a tagfile for the corpus and store the result in a file.
    */
    Expected<void>
    buildTagfile(
        std::string_view fileName,
        Corpus const& corpus) const;

    /** Output an escaped string to the output stream.
    */
    virtual
    void
    escape(OutputRef& os, std::string_view str) const;

protected:
    /** Customize the Handlebars corpus before rendering.

        Subclasses can override this to inject additional data
        into the corpus (e.g. precomputed template context).
        The default implementation does nothing.
    */
    virtual
    void
    prepareCorpus(HandlebarsCorpus&) const;

protected:
    /** Default stylesheet path on disk; empty means no default. */
    virtual std::string defaultStylesheetSource(Config const& config) const;

    /** Default stylesheet output relative path; empty means no default. */
    virtual std::string defaultStylesheetOutput(Config const& config) const;

    /** Default highlight stylesheet path on disk; empty means no default. */
    virtual std::string defaultHighlightStylesheetSource(Config const& config) const;

    /** Default highlight stylesheet output relative path; empty means no default. */
    virtual std::string defaultHighlightStylesheetOutput(Config const& config) const;

    /** Inline script used to load and run highlight.js from a CDN. */
    virtual std::string defaultHighlightScript() const;
};

} // hbs

} // mrdocs

#endif
