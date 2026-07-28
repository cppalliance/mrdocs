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

#include <lib/Gen/hbs/HandlebarsCorpus.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <map>
#include <ostream>
#include <vector>


namespace mrdocs {
namespace hbs {

/** Per-thread renderer for Handlebars output.

    A `HandlebarsGenerator` spins up one `Builder` per worker thread to
    keep template state, JS contexts, and caches isolated while the DOM
    visitors walk symbols in parallel. The generator itself orchestrates
    traversal and output paths, while `Builder` focuses solely on taking a
    single symbol (or a custom contents callback) and rendering the
    appropriate Handlebars templates using the prepared `HandlebarsCorpus`.

    Separating the renderer from the generator avoids cross-thread
    contention on Handlebars state and keeps rendering concerns out of the
    generator’s coordination logic.
*/
class Builder
{
    js::Context ctx_;
    lua::Context lua_ctx_;
    Handlebars hbs_;
    std::map<std::string, std::string, std::less<>> templates_;
    std::function<void(OutputRef&, std::string_view)> escapeFn_;

public:
    HandlebarsCorpus const& domCorpus;

    explicit
    Builder(
        HandlebarsCorpus const& corpus,
        std::function<void(OutputRef&, std::string_view)> escapeFn);

    /** Render the contents for a symbol.

        If the output is single page or embedded,
        this function renders the index template
        with the symbol.

        If the output is multi-page and not embedded,
        this function renders the wrapper template
        with the index template as the contents.

        @param os Stream to receive rendered output.
        @param I  Metadata symbol to render.
        @return Success or an error describing template or I/O failures.

        @par Example
        @code
        Builder b(corpus, Handlebars::htmlEscape);
        std::ostringstream out;
        b(out, *corpus.root()); // writes HTML/Adoc for the root symbol
        @endcode
    */
    template<std::derived_from<Symbol> T>
    Expected<void>
    operator()(std::ostream& os, T const&);

    /** Render the contents in the wrapper layout.

        This function will render the contents
        of the wrapper template.

        When the {{contents}} are rendered in the
        wrapper template, the specified function
        will be executed to render the contents
        of the page.

        @param os         Stream to receive rendered output.
        @param contentsCb Callback invoked to write the inner page
                           contents inside the wrapper layout.
        @return Success or an error from template rendering or the
                callback.

        @par Example
        @code
        b.renderWrapped(out, [&] {
            return b.callTemplate(out, "index.html.hbs", ctx);
        });
        @endcode

    */
    Expected<void>
    renderWrapped(
        std::ostream& os,
        std::function<Expected<void>()> contentsCb);

private:
    /** Path to the index template file resolved for the active generator. */
    std::string
    indexTemplateFile() const;

    /** Path to the wrapper (layout) template file when multi-page output is used. */
    std::string
    wrapperTemplateFile() const;

    /** Create a handlebars context with the symbol and helper information.

        The helper information includes all information from the
        config file, plus the symbol information.

        It also includes a sectionref helper that describes
        the section where the symbol is located.

        @param I Symbol to expose to the template.
        @return A DOM object with `page`, `symbol`, and `config` nodes
                ready for Handlebars rendering.
    */
    dom::Object
    createContext(Symbol const& I);

    /** Render a Handlebars template from the templates directory.

        @param os      Output stream to receive rendered bytes.
        @param name    Template filename (as registered in `templates_`).
        @param context DOM data passed into Handlebars.
        @return Success or an error describing template failures.
    */
    Expected<void>
    callTemplate(
        std::ostream& os,
        std::string_view name,
        dom::Value const& context);

};

} // hbs
} // mrdocs


#endif // MRDOCS_LIB_GEN_HBS_BUILDER_HPP
