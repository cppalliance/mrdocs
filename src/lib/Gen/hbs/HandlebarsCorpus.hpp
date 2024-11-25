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
#include "lib/Support/LegibleNames.hpp"
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <optional>

namespace clang {
namespace mrdocs {
namespace hbs {

/** A specialized DomCorpus for generating Handlebars values.

    This class extends @ref DomCorpus to provide
    additional functionality specific to Handlebars
    generation.

*/
class HandlebarsCorpus : public DomCorpus
{
public:
    /** Legible names for the Handlebars corpus. */
    LegibleNames names_;

    /** File extension for the markup files. */
    std::string fileExtension;

    /** Function to convert a Javadoc node to a string. */
    std::function<std::string(HandlebarsCorpus const&, doc::Node const&)> toStringFn;

    /** Constructor.

        Initializes the HandlebarsCorpus with the given corpus and options.

        @param corpus The base corpus.
        @param fileExtension The file extension for the generated files.
        @param toStringFn The function to convert a Javadoc node to a string.
    */
    HandlebarsCorpus(
        Corpus const& corpus,
        std::string_view fileExtension,
        std::function<std::string(HandlebarsCorpus const&, doc::Node const&)> toStringFn)
        : DomCorpus(corpus)
        , names_(corpus, corpus.config->legibleNames)
        , fileExtension(fileExtension)
        , toStringFn(std::move(toStringFn))
    {
    }

    /** Construct a dom::Object from the given Info.

        @param I The Info object to construct from.
        @return A dom::Object representing the Info.
    */
    dom::Object
    construct(Info const& I) const override;

    /** Construct a dom::Object from the given OverloadSet.

        @param os The OverloadSet to get the overloads for.
        @return A dom::Object representing the overloads.
    */
    dom::Object
    construct(
        OverloadSet const& os) const override;

    /** Get the cross-reference for the given Info.

        @param I The Info object to get the cross-reference for.
        @return A string representing the cross-reference.
    */
    template<class T>
    requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
    std::string
    getURL(T const& I) const;

    /** Return a Dom value representing the Javadoc.

        @param jd The Javadoc object to get the Javadoc for.
        @return A dom::Value representing the Javadoc.
    */
    dom::Value
    getJavadoc(Javadoc const& jd) const override;
};

} // hbs
} // mrdocs
} // clang

#endif
