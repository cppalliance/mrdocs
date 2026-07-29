//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_ENGINE_HPP
#define MRDOCS_API_HANDLEBARS_ENGINE_HPP

// The Handlebars engine class, plus the isEmpty/createFrame/escapeExpression helpers and the helpers:: registration API.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Error.hpp>
#include <mrdocs/Handlebars/Platform.hpp>
#include <mrdocs/Handlebars/OutputRef.hpp>
#include <mrdocs/Handlebars/Options.hpp>
#include <mrdocs/Handlebars/detail/Engine.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mrdocs {
namespace handlebars {

/** A handlebars environment

    This class implements a handlebars template environment.

    It is analogous to the complete state held by the handlebars.js
    module, including registered helpers and partials.

    In the general case, handlebars.js provides a global `Handlebars`
    environment where helpers and partials are registered:

    @code{.js}
      let template = Handlebars.compile("{{foo}}");
      let result = template({foo: "bar"});
    @endcode

    but also provides a way to create a new isolated environment with its own
    helpers and partials:

    @code{.js}
      let OtherHandlebars = Handlebars.create();
      let template = OtherHandlebars.compile("{{foo}}");
      let result = template({foo: "bar"});
    @endcode

    In this implementation, however, there's no global environment.
    A new environment needs to be created explicitly be instantiating
    this class:

    @code{.cpp}
      Handlebars env;
      dom::Object context;
      context["foo"] = "bar";
      std::string result = env.render("{{ foo }}", context);
      assert(result == "bar");
    @endcode

    A handlebars template can be rendered using the context data provided
    as a `dom::Value`, which is usually a `dom::Object` at the first level
    when calling `render`.

    In the most general case, the result can returned as a string or rendered
    directly to a buffer or stream. The render function provides an overload
    that allows the caller to provide an output stream where the template
    will be rendered directly without allocating a string:

    @code{.cpp}
      Handlebars env;
      dom::Object context;
      context["foo"] = "bar";
      env.render_to(std::cout, "{{ foo }}", context);
      // prints "bar" to stdout
    @endcode

    @code{.cpp}
      Handlebars env;
      dom::Object context;
      context["foo"] = "bar";
      std::string result;
      env.render_to(result, "{{ foo }}", context);
      assert(result == "bar");
    @endcode

    Design considerations:

    The following notes include some design considerations for the current
    implementation of this class. These are intended to describe the
    current state of the class rather than to provide the final specification
    of the class behavior.

    Compiled templates:

    Unlike handlebars.js, this implementation renders the template
    directly to the output stream, without requiring an intermediary
    object to store a representation of the compiled template
    (`Handlebars.precompile`) or an intermediary callable object
    required to ultimately render the template (`Handlebars.precompile`).

    The rationale is that there is not much benefit in pre-compiling templates
    in C++, since both iterating the input string and a pre-compiled template
    would have very similar costs even in optimal implementations of the
    compiled template.

    The most significant benefit of pre-compiling templates in C++ would
    be the faster identification of the ends of blocks, which would
    allow the engine iterate the block only once. For this reason,
    compiled templates will still be considered in a future version
    of this sub-library.

    Also note that compiled templates cannot avoid exceptions, because
    a compiled template can still invoke a helper that throws exceptions
    and evaluate dynamic expressions that cannot be identified during the
    first pass.

    Incremental rendering and compilation:

    Although this is not supported by handlebars.js, it's common for
    C++ template engines to support incremental rendering, where the
    template is compiled or rendered in chunks as it is parsed.
    This implementation does not yet support this feature.

    This is useful for streaming templates, where the template is
    rendered to a stream as it is parsed, without requiring the
    entire template to be parsed and compiled before rendering
    starts.

    There are two types of incremental rendering and compilation
    supported by this implementation:

    - Incremental rendering of a partial template input to a stream
    - Incremental rendering into an output buffer of fixed size

    In each of these cases, the template is rendered in chunks
    until the end of the partial template is reached or the output buffer
    is full.

    In a scenario with compiled templates, the complexity of incremental
    rendering needs to be implemented for both compilation and rendering.

    The main difficulty to implement incremental rendering for handlebars.js
    is that helpers can be invoked from anywhere in the template, and
    most content is actually rendered by helpers. This means that
    helpers would need to be able to interoperate with whatever mechanism
    is designed to support suspension in this recursive-coroutine-like
    interface.

    Error propagation:

    The main logic to render a template is implemented in the `render`
    function, does not throws by itself. How identifying the next tag
    in a template string, the algorithms uses a loose implementation
    where unclosed tags are rendered as-is instead of throwing errors.

    However, helpers are allowed to throw exceptions to propagate errors,
    so the `render` function is not `noexcept`.

    For this reason, exceptions thrown by helpers are in fact exceptional
    conditions that should be handled by the caller. In general,
    exceptions can be avoided completely by not throwing exceptions from
    helpers.

    @see https://handlebarsjs.com/
*/
class Handlebars {
    using helpers_map = std::unordered_map<
        std::string, dom::Function, detail::string_hash, std::equal_to<>>;

    using partials_map = detail::partials_map;
    partials_map partials_;
    helpers_map helpers_;
    dom::Function logger_;

public:
    /** Construct a handlebars environment

        This constructor creates a new handlebars environment with the
        built-in helpers and default logger.

        Each environment has its own helpers and partials. Multiple
        environments are only necessary for use cases that demand distinct
        helpers or partials.

        @see helpers::registerBuiltinHelpers
    */
    Handlebars();

    /** Render a handlebars template

        This function renders the specified handlebars template and
        returns the result as a string.

        The context data to render is passed as a dom::Value, which is
        usually a dom::Object at the first level. When the context is
        not an object, it is accessed from the handlebars template with
        the `this` keyword.

        @param templateText The handlebars template text
        @param context The data to render
        @param options The options to use
        @return The rendered text
    */
    std::string
    render(
        std::string_view templateText,
        dom::Value const& context,
        HandlebarsOptions const& options) const
    {
        auto exp = try_render(templateText, context, options);
        if (!exp)
        {
            throw exp.error();
        }
        return *exp;
    }

    /// @overload
    std::string
    render(
        std::string_view templateText,
        dom::Value const& context) const
    {
        auto exp = try_render(templateText, context, {});
        if (!exp)
        {
            throw exp.error();
        }
        return *exp;
    }

    /// @overload
    std::string
    render(std::string_view templateText) const
    {
        dom::Value emptyContext(dom::Object{});
        HandlebarsOptions defaultOptions;
        auto exp = try_render(templateText, emptyContext, defaultOptions);
        if (!exp)
        {
            throw exp.error();
        }
        return *exp;
    }

    /** Render a handlebars template

        This function renders the specified handlebars template and
        writes the result to the specified output stream.

        The output stream can be any type convertible to OutputRef, which is
        a reference to a stream that can be written to with the << operator.

        @param out The output stream reference
        @param templateText The handlebars template text
        @param context The data to render
        @param options The options to use
        @return The rendered text
    */
    void
    render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context,
        HandlebarsOptions const& options) const
    {
        auto exp = try_render_to(out, templateText, context, options);
        if (!exp)
        {
            throw exp.error();
        }
    }

    /// @overload
    void
    render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context) const
    {
        auto exp = try_render_to(out, templateText, context, {});
        if (!exp)
        {
            throw exp.error();
        }
    }

    /// @overload
    void
    render_to(
        OutputRef& out,
        std::string_view templateText) const
    {
        dom::Object const& context = {};
        auto exp = try_render_to(out, templateText, context, {});
        if (!exp)
        {
            throw exp.error();
        }
    }

    /** @copydoc render_to(OutputRef&, std::string_view, dom::Value const&, HandlebarsOptions const&) const
    */
    Expected<std::string, Error>
    try_render(
        std::string_view templateText,
        dom::Value const& context,
        HandlebarsOptions const& options) const
    {
        std::string out;
        OutputRef os(out);
        auto exp = try_render_to(os, templateText, context, options);
        if (!exp)
        {
            return Unexpected(exp.error());
        }
        return out;
    }

    /// @overload
    Expected<std::string, Error>
    try_render(
        std::string_view templateText,
        dom::Value const& context) const
    {
        return try_render(templateText, context, {});
    }

    /// @overload
    Expected<std::string, Error>
    try_render(std::string_view templateText) const
    {
        dom::Object const& context = {};
        return try_render(templateText, context, {});
    }

    /** Render a handlebars template

        This function renders the specified handlebars template and
        writes the result to the specified output stream.

        The output stream can be any type convertible to OutputRef, which is
        a reference to a stream that can be written to with the << operator.

        @param out The output stream reference
        @param templateText The handlebars template text
        @param context The data to render
        @param options The options to use
        @return The rendered text
    */
    Expected<void, Error>
    try_render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context,
        HandlebarsOptions const& options) const;

    /// @overload
    Expected<void, Error>
    try_render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context) const
    {
        return try_render_to(out, templateText, context, {});
    }

    /// @overload
    Expected<void, Error>
    try_render_to(
        OutputRef& out,
        std::string_view templateText) const
    {
        dom::Object const& context = {};
        return try_render_to(out, templateText, context, {});
    }

    /** Register a partial

        This function registers a partial with the handlebars environment.

        A partial is a template that can be referenced from another
        template. The partial is rendered in the context of the
        template that references it.

        For instance, a partial can be used to render a header or
        footer that is common to several pages. It can also be used
        to render a list of items that is used in several places.

        The following example template uses the partial `item` to render a
        list of items:

        @code{.handlebars}
        <ul>
        {{#each items}}
            {{> item}}
        {{/each}}
        </ul>
        @endcode

        @param name The name of the partial
        @param text The content of the partial

        @see https://handlebarsjs.com/guide/partials.html

    */
    void
    registerPartial(std::string_view name, std::string_view text);

    /** Unregister a partial

        This function unregisters a partial with the handlebars environment.

        @param name The name of the partial
    */
    void
    unregisterPartial(std::string_view name) {
        auto it = partials_.find(name);
        if (it != partials_.end())
        {
            partials_.erase(it);
        }
    }

    /** Register a helper accessible by any template in the environment.

        The helper type is a type erased function of type @ref dom::Function,
        which receives the resolved template arguments as parameters as
        a @ref dom::Value for each parameter.

        The helper function also receives an object populated with variables
        that are accessible in the current context as its N+1-th parameter.
        This object contains information about the current context and is
        useful for helpers that want to change the current context or render
        internal blocks.

        As all instances of @ref dom::Function, the helper should also return
        a @ref dom::Value. If the function semantics does not require a return
        value, the function should return a @ref dom::Value of type
        @ref dom::Kind::Undefined.

        When the helper is used in an subexpression, the @ref dom::Value return
        value is used as the intermediary result. When the helper is used in
        a block or a final expression, the @ref dom::Value return value will be
        formatted to the output.

        @param name The name of the helper in the handlebars template
        @param helper The helper function

        @see https://handlebarsjs.com/guide/expressions.html
        @see https://handlebarsjs.com/guide/block-helpers.html
        @see https://handlebarsjs.com/guide/builtin-helpers.html
    */
    void
    registerHelper(std::string_view name, dom::Function const& helper);

    /** Unregister a helper

        This function unregisters a helper with the handlebars environment.

        @param name The name of the helper
    */
    void
    unregisterHelper(std::string_view name);

    /** Register a logger

        This function registers a logger with the handlebars environment.
        A logger is a function that is called from the built-in
        "log" helper function.

        The logger can also be called from any helper through the
        `HandlebarsCallback` parameter.

        The logger function is called with a `dom::Value` indicating the
        current level and a `dom::Array` containing values to be logged.

        @param fn The logger function
    */
    void
    registerLogger(dom::Function fn);

    /** Internal representation of a parsed Handlebars tag.
    */
    struct Tag;

private:
    // render to ostream using extra partials from parent contexts
    Expected<void, Error>
    try_render_to_impl(
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, Error>
    renderTag(
        Tag const& tag,
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, Error>
    renderBlock(
        std::string_view blockName,
        Handlebars::Tag const &tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state,
        bool isChainedBlock) const;

    Expected<void, Error>
    renderPartial(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, Error>
    renderDecorator(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, Error>
    renderExpression(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, Error>
    setupArgs(
        std::string_view expression,
        dom::Value const& context,
        detail::RenderState& state,
        dom::Array &args,
        dom::Object& cb,
        HandlebarsOptions const& opt) const;

    struct evalExprResult {
        dom::Value value;
        bool found = false;
        bool isLiteral = false;
        bool isSubexpr = false;
        bool fromBlockParams = false;
    };

    [[nodiscard]]
    Expected<evalExprResult, Error>
    evalExpr(
        dom::Value const &context,
        std::string_view expression,
        detail::RenderState &state,
        HandlebarsOptions const& opt,
        bool evalLiterals) const;

    std::pair<dom::Function, bool>
    getHelper(std::string_view name, bool isBlock) const;

    std::pair<std::string_view, bool>
    getPartial(
        std::string_view name,
        detail::RenderState const& state) const;
};

/** Determine if a value is empty

    This is used by the built-in if and with helpers to control their
    execution flow.

    The Handlebars definition of empty is any of:

    - Array with length 0
    - falsy values other than 0

    This is intended to match the Mustache Behaviour.

    @param arg The value to test
    @return True if the value is empty, false otherwise

    @see https://mustache.github.io/mustache.5.html#Sections
*/
MRDOCS_HANDLEBARS_DECL
bool
isEmpty(dom::Value const& arg);

/** Create child data objects.

    This function can be used by block helpers to create child
    data objects.

    The child data object is an overlay frame object implementation
    that will first look for a value in the child object and if
    not found will look in the parent object.

    Helpers that modify the data state should create a new frame
    object when doing so, to isolate themselves and avoid corrupting
    the state of any parents.

    Generally, only one frame needs to be created per helper
    execution. For example, the each iterator creates a single
    frame which is reused for all child execution.

    @param parent The underlying frame object
    @return The overlay object

    @see https://mustache.github.io/mustache.5.html#Sections
*/
MRDOCS_HANDLEBARS_DECL
dom::Object
createFrame(dom::Object const& parent);

/// @overload
MRDOCS_HANDLEBARS_DECL
dom::Object
createFrame(dom::Value const& parent);

/// @overload
dom::Object
createFrame(dom::Object const& child, dom::Object const& parent);

/// @overload escapeExpression(std::string_view)
MRDOCS_HANDLEBARS_DECL
void
escapeExpression(
    OutputRef out,
    std::string_view str,
    HandlebarsOptions const& opt);


} // namespace handlebars
} // namespace mrdocs

#endif
