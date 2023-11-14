//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ADOC_ADOCCORPUS_HPP
#define MRDOCS_LIB_ADOC_ADOCCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include "lib/Support/SafeNames.hpp"
#include "Options.hpp"
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <optional>

namespace clang {
namespace mrdocs {
namespace adoc {

class AdocCorpus : public DomCorpus
{
public:
    Options options;
    SafeNames names_;

    AdocCorpus(
        Corpus const& corpus,
        Options&& opts)
        : DomCorpus(corpus)
        , options(std::move(opts))
        , names_(corpus, options.safe_names)
    {
    }

    dom::Object
    construct(Info const& I) const override;

    std::string
    getXref(Info const& I) const;

    std::string
    getXref(OverloadSet const& os) const;

    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;

    dom::Object
    getOverloads(
        OverloadSet const& os) const override;
};

} // adoc
} // mrdocs
} // clang

#endif
