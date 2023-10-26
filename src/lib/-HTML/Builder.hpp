//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_HTML_BUILDER_HPP
#define MRDOCS_LIB_HTML_BUILDER_HPP

#include "Options.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <ostream>

namespace clang {
namespace mrdocs {
namespace html {

/** Builds reference output.

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    DomCorpus const& domCorpus_;
    Corpus const& corpus_;
    Options options_;
    js::Context ctx_;
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

} // html
} // mrdocs
} // clang

#endif
