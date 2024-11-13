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

    /** Render a Handlebars template from the templates directory.
     */
    Expected<std::string>
    callTemplate(
        std::string_view name,
        dom::Value const& context);

    /** Render the header for a single page.
     */
    Expected<std::string>
    renderSinglePageHeader();

    /** Render the footer for a single page.
     */
    Expected<std::string>
    renderSinglePageFooter();

    /** Render the contents for a symbol.
     */
    template<class T>
    Expected<std::string>
    operator()(T const&);

    /** Render the contents for an overload set.
     */
    Expected<std::string>
    operator()(OverloadSet const&);
};

} // hbs
} // mrdocs
} // clang

#endif
