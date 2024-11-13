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

/** Builds reference output as a string for any Info type

    This contains all the state information
    for a single thread to generate output.
*/
class Builder
{
    js::Context ctx_;
    Handlebars hbs_;

    std::string
    getRelPrefix(std::size_t depth);

public:
    HandlebarsCorpus const& domCorpus;

    explicit
    Builder(HandlebarsCorpus const& corpus);

    /** Render the contents for a symbol.
     */
    template<class T>
    Expected<std::string>
    operator()(T const&);

    /** Render the contents for an overload set.
     */
    Expected<std::string>
    operator()(OverloadSet const&);

    /** Wrap the contents of a page in the page template.
     */
    Expected<void>
    wrapPage(
        std::ostream& out,
        std::istream& in);

private:
    /** Create a handlebars context with the symbol and helper information.

        The helper information includes all information from the
        config file, plus the symbol information.

        It also includes a sectionref helper that describes
        the section where the symbol is located.
     */
    dom::Object
    createContext(Info const& I);

    /// @copydoc createContext(Info const&)
    dom::Object
    createContext(OverloadSet const& OS);

    /** Render a Handlebars template from the templates directory.
     */
    Expected<std::string>
    callTemplate(
        std::string_view name,
        dom::Value const& context);

};

} // hbs
} // mrdocs
} // clang

#endif
