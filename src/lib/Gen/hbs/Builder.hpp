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

#ifndef MRDOCS_LIB_GEN_HBS_BUILDER_HPP
#define MRDOCS_LIB_GEN_HBS_BUILDER_HPP

#include "Options.hpp"
#include "HandlebarsCorpus.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <ostream>

namespace clang {
namespace mrdocs {
namespace hbs {

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
    HandlebarsCorpus const& domCorpus;

    explicit
    Builder(
        HandlebarsCorpus const& corpus);

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

} // hbs
} // mrdocs
} // clang

#endif
