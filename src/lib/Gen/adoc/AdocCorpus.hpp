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

#ifndef MRDOCS_LIB_GEN_ADOC_ADOCCORPUS_HPP
#define MRDOCS_LIB_GEN_ADOC_ADOCCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include "lib/Support/LegibleNames.hpp"
#include "Options.hpp"
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <optional>

namespace clang {
namespace mrdocs {
namespace adoc {

/** A specialized DomCorpus for generating Adoc nodes.

    This class extends @ref DomCorpus to provide
    additional functionality specific to Adoc generation.
*/
class AdocCorpus : public DomCorpus
{
public:
    /** Options for the Adoc corpus. */
    Options options;

    /** Legible names for the Adoc corpus. */
    LegibleNames names_;

    /** Constructor.

        Initializes the AdocCorpus with the given corpus and options.

        @param corpus The base corpus.
        @param opts Options for the Adoc corpus.
    */
    AdocCorpus(
        Corpus const& corpus,
        Options&& opts)
        : DomCorpus(corpus)
        , options(std::move(opts))
        , names_(corpus, options.legible_names)
    {
    }

    /** Construct a dom::Object from the given Info.

        @param I The Info object to construct from.
        @return A dom::Object representing the Info.
    */
    dom::Object
    construct(Info const& I) const override;

    /** Get the cross-reference for the given Info.

        @param I The Info object to get the cross-reference for.
        @return A string representing the cross-reference.
    */
    std::string
    getXref(Info const& I) const;

    /** Get the cross-reference for the given OverloadSet.

        @param os The OverloadSet to get the cross-reference for.
        @return A string representing the cross-reference.
    */
    std::string
    getXref(OverloadSet const& os) const;

    /** Return a Dom value representing the Javadoc.

        @param jd The Javadoc object to get the Javadoc for.
        @return A dom::Value representing the Javadoc.
    */
    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;

    /** Return a Dom value representing an overload set.

        @param os The OverloadSet to get the overloads for.
        @return A dom::Object representing the overloads.
    */
    dom::Object
    getOverloads(
        OverloadSet const& os) const override;
};

} // adoc
} // mrdocs
} // clang

#endif
