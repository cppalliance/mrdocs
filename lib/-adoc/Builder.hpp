//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_BUILDER_HPP
#define MRDOX_LIB_ADOC_BUILDER_HPP

#include "Options.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/JavaScript.hpp>
#include <ostream>

namespace clang {
namespace mrdox {
namespace adoc {

/** Builds reference output.

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    DomCorpus const& domCorpus_;
    Corpus const& corpus_;
    Options options_;
    Handlebars hbs_;

public:
    Builder(
        DomCorpus const& domCorpus,
        Options const& options);

    dom::Value createContext(SymbolID const& id);

    Expected<std::string>
    callTemplate(
        std::string_view name,
        dom::Value const& context);

    Expected<std::string> renderSinglePageHeader();
    Expected<std::string> renderSinglePageFooter();

    template<class T>
    Expected<std::string>
    operator()(T const&);
};

} // adoc
} // mrdox
} // clang

#endif
