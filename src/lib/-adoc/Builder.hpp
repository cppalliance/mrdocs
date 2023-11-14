//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_ADOC_BUILDER_HPP
#define MRDOCS_LIB_ADOC_BUILDER_HPP

#include "Options.hpp"
#include "AdocCorpus.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <ostream>

#include <mrdocs/Dom.hpp>

namespace clang {
namespace mrdocs {
namespace adoc {

/** Builds reference output.

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    js::Context ctx_;
    Handlebars hbs_;

    std::string getRelPrefix(std::size_t depth);

public:
    AdocCorpus const& domCorpus;

    explicit
    Builder(
        AdocCorpus const& corpus);

    dom::Value createContext(Info const& I);
    dom::Value createContext(OverloadSet const& OS);

    Expected<std::string>
    callTemplate(
        std::string_view name,
        dom::Value const& context);

    Expected<std::string> renderSinglePageHeader();
    Expected<std::string> renderSinglePageFooter();

    template<class T>
    Expected<std::string>
    operator()(T const&);

    Expected<std::string>
    operator()(OverloadSet const&);
};

} // adoc
} // mrdocs
} // clang

#endif
