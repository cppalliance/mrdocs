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

        If the output is single page or embedded,
        this function renders the index template
        with the symbol.

        If the output is multi-page and not embedded,
        this function renders the wrapper template
        with the index template as the contents.
     */
    template<class T>
    Expected<std::string>
    operator()(T const&);

    /// @copydoc operator()(T const&)
    Expected<std::string>
    operator()(OverloadSet const&);

    /** Render the contents in the wrapper layout.

        This function will render the contents
        of the wrapper template.

        When the {{contents}} are rendered in the
        wrapper template, the specified function
        will be executed to render the contents
        of the page.

     */
    Expected<void>
    renderWrapped(std::ostream& os, std::function<Error()> contentsCb);

private:
    /** The directory with the all templates.
     */
    std::string
    templatesDir() const;

    /** A subdirectory of the templates dir
     */
    std::string
    templatesDir(std::string_view subdir) const;

    /** The directory with the common templates.
     */
    std::string
    commonTemplatesDir() const;

    /** A subdirectory of the common templates dir
     */
    std::string
    commonTemplatesDir(std::string_view subdir) const;

    /** The directory with the layout templates.
     */
    std::string
    layoutDir() const;

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
