//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_HTML_HTMLCORPUS_HPP
#define MRDOCS_LIB_HTML_HTMLCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DomMetadata.hpp>

namespace clang {
namespace mrdocs {
namespace html {

class HTMLCorpus : public DomCorpus
{
public:
    explicit
    HTMLCorpus(
        Corpus const& corpus)
        : DomCorpus(corpus)
    {
    }

    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;
};

} // html
} // mrdocs
} // clang

#endif
