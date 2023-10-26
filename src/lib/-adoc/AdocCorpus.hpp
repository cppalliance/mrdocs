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
    std::optional<SafeNames> safe_names;

    AdocCorpus(
        Corpus const& corpus,
        Options&& opts)
        : DomCorpus(corpus)
        , options(std::move(opts))
    {
        if(options.safe_names)
            safe_names.emplace(corpus);
    }

    dom::Object
    construct(Info const& I) const override;

    std::string
    getXref(SymbolID const& id) const;

    dom::Value
    getJavadoc(
        Javadoc const& jd) const override;
};

} // adoc
} // mrdocs
} // clang

#endif
