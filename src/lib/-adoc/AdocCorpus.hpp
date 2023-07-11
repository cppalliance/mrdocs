//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_ADOCCORPUS_HPP
#define MRDOX_LIB_ADOC_ADOCCORPUS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/DomMetadata.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocCorpus : public DomCorpus
{
public:
    explicit
    AdocCorpus(
        Corpus const& corpus)
        : DomCorpus(corpus)
    {
    }

    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;
};

} // adoc
} // mrdox
} // clang

#endif
