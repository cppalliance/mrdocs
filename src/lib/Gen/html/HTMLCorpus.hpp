//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HTML_HTMLCORPUS_HPP
#define MRDOCS_LIB_GEN_HTML_HTMLCORPUS_HPP

#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Platform.hpp>

namespace clang {
namespace mrdocs {
namespace html {

/** A specialized DomCorpus for generating HTML nodes.

    This class extends @ref DomCorpus to provide
    additional functionality specific to HTML generation.
*/
class HTMLCorpus : public DomCorpus
{
public:
    /** Constructor.

        Initializes the HTMLCorpus with the given corpus and options.

        @param corpus The base corpus.
        @param opts Options for the HTML corpus.
    */
    explicit
    HTMLCorpus(
        Corpus const& corpus)
        : DomCorpus(corpus)
    {
    }

    /** Return a Dom value representing the Javadoc.

        @param jd The Javadoc object to get the Javadoc for.
        @return A dom::Value representing the Javadoc.
    */
    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;
};

} // html
} // mrdocs
} // clang

#endif
