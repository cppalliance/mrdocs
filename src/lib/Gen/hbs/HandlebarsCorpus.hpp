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

#ifndef MRDOCS_LIB_GEN_HBS_HANDLEBARSCORPUS_HPP
#define MRDOCS_LIB_GEN_HBS_HANDLEBARSCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Support/LegibleNames.hpp>
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
    /** Legible names for the Handlebars corpus.
    */
    LegibleNames names_;

    /** File extension for the markup files.
    */
    std::string fileExtension;

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
        std::string_view fileExtension)
        : DomCorpus(corpus)
        , names_(corpus, corpus.config->legibleNames)
        , fileExtension(fileExtension)
    {
    }

    /** Construct a dom::Object from the given Info.

        @param I The Info object to construct from.
        @return A dom::Object representing the Info.
    */
    dom::Object
    construct(Symbol const& I) const override;

    /** Get the cross-reference for the given Info.

        @param I The Info object to get the cross-reference for.
        @return A string representing the cross-reference.
    */
    std::string
    getURL(Symbol const& I) const;
};

} // mrdocs::hbs

#endif // MRDOCS_LIB_GEN_HBS_HANDLEBARSCORPUS_HPP
