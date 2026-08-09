//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GENERATORS_HBS_HANDLEBARSCORPUS_HPP
#define MRDOCS_LIB_GENERATORS_HBS_HANDLEBARSCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>

namespace mrdocs::hbs {

/** A specialized DomCorpus for generating Handlebars values.

    This class extends @ref DomCorpus to provide
    additional functionality specific to Handlebars
    generation.

*/
class HandlebarsCorpus final : public DomCorpus
{
public:
    /** File extension for the markup files.
    */
    std::string fileExtension;

    /** The configuration driving this render pass (owned by the caller).
     */
    Config const& config_;

    /** Stylesheets to load for the page.

        Array of objects with `path` (href or output-relative link) and
        `external` (true for remote URLs). Populated by the generator with
        resolved link targets, not source file paths.
    */
    dom::Array stylesheets;

    /** Inline stylesheet contents (optional).

        Array of raw CSS strings that may be inlined into the wrapper.
    */
    dom::Array inlineStyles;

    /** Inline scripts to inject into the page head (optional).
     */
    dom::Array inlineScripts;

    /** True when the generator supplied the bundled default styles.
     */
    bool hasDefaultStyles = false;

    /** Constructor.

        Initializes the HandlebarsCorpus with the given corpus and options.

        @param corpus The base corpus.
        @param fileExtension The file extension for the generated files.
        @param toStringFn The function to convert a DocComment node to a string.
    */
    HandlebarsCorpus(
        Corpus const& corpus,
        Config const& config,
        std::string_view fileExtension)
        : DomCorpus(corpus)
        , fileExtension(fileExtension)
        , config_(config)
    {
    }

    /** The configuration driving this render pass.

        This render pass holds the @ref Config it was given; the caller
        that starts the pass owns it and keeps it alive for the pass.
    */
    Config const& config() const noexcept { return config_; }

    /** Construct a dom::Object from the given Info.

        @param I The Info object to construct from.
        @return A dom::Object representing the Info.
    */
    dom::Object
    construct(Symbol const& I) const override;

    /** Get the page URL for a symbol, or empty if it has none.

        This is the URL to link to when referring to `I`. It returns the
        symbol's own page URL when the symbol is generated, otherwise the
        URL of the primary template it is documented under
        (specializations and dependencies), or an empty string when
        neither applies. It backs the `getUrl` Handlebars helper.

        @param I The symbol to link to.
        @return The page URL, or an empty string when the symbol has no
                page and no documenting primary.
    */
    std::string
    getURL(Symbol const& I) const;

    /** Get the qualified anchor for a symbol, joined with `delim`.

        Walks the parent chain and joins the per-symbol @ref Symbol::Anchor
        values with `delim` (or returns the flat hash when legible names
        are disabled). This is the page-unique reference for the symbol:
        @ref getCanonicalURL builds on it with '/' for multipage paths or
        '-' for single-page fragments, and templates use the '-' form as
        the heading/fragment id, which must be unique on a single page even
        though @ref Symbol::Anchor stores only the unqualified name.

        It is public because heading ids are emitted per symbol, not just
        per page, and cannot come from @ref getURL: an id is the '-'-joined
        qualified name, which differs from a multipage '/' path.

        It is a C++ function rather than a recursive Handlebars partial (the way
        qualified names are rendered as text) because a heading id is a
        hash-argument attribute value (markup/h `id=`); a partial renders
        content, not a value, so it cannot fill an attribute.

        @param I The symbol whose qualified anchor to build.
        @param delim The separator joining ancestor anchors.
        @return The qualified anchor for `I`.
    */
    std::string
    getQualifiedAnchor(Symbol const& I, char delim) const;

private:
    /** Get the canonical page URL for a symbol, assuming it is generated.

        Builds the URL purely from the symbol's legible name, output mode
        (multipage path vs single-page anchor) and file extension, without
        checking whether the symbol is actually generated. @ref getURL is
        the public entry point; it uses this after deciding which symbol's
        page to point at.

        @param I The symbol whose canonical URL to build.
        @return The canonical page URL for `I`.
    */
    std::string
    getCanonicalURL(Symbol const& I) const;
};

} // mrdocs::hbs

#endif // MRDOCS_LIB_GENERATORS_HBS_HANDLEBARSCORPUS_HPP
