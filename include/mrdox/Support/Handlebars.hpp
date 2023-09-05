//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_SUPPORT_PATH_HPP
#define MRDOX_TOOL_SUPPORT_PATH_HPP

#include <mrdox/Support/String.hpp>
#include <mrdox/Dom.hpp>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <vector>
#include <variant>

namespace clang {
namespace mrdox {

/** Options for handlebars

    In particular, we have the noHTMLEscape option, which we
    use to disable HTML escaping when rendering asciidoc templates.

    This struct is analogous to the Handlebars.compile options.

 */
struct HandlebarsOptions
{
    /** Escape HTML entities
     */
    bool noEscape = false;

    /** Templates will throw rather than ignore missing fields

        Run in strict mode. In this mode, templates will throw rather than
        silently ignore missing fields.

     */
    bool strict = false;

    /** Removes object existence checks when traversing paths

        This is a subset of strict mode that generates optimized
        templates when the data inputs are known to be safe.
     */
    bool assumeObjects = false;

    /** Disable the auto-indent feature

        By default, an indented partial-call causes the output of the
        whole partial being indented by the same amount.
     */
    bool preventIndent = false;

    /** Disables standalone tag removal when set to true

        When set, blocks and partials that are on their own line will not
        remove the whitespace on that line.
     */
    bool ignoreStandalone = false;

    /** Disables implicit context for partials

        When enabled, partials that are not passed a context value will
        execute against an empty object.
     */
    bool explicitPartialContext = false;

    /** Enable recursive field lookup

        When enabled, fields will be looked up recursively in objects
        and arrays.

        This mode should be used to enable complete compatibility
        with Mustache templates.
     */
    bool compat = false;

    /** Enable tracking of ids

        When enabled, the ids of the expressions are tracked and
        passed to the helpers.

        Helpers often use this information to update the context
        path to the current expression, which can later be used to
        look up the value of the expression with ".." segments.
     */
    bool trackIds = false;

    /** Custom private data object

        This variable can be used to pass in an object to define custom
        private variables.
     */
    dom::Value data = nullptr;
};

namespace detail {
    // Objects such as llvm::raw_string_ostream
    template <typename Os>
    concept LHROStreamable =
        requires(Os &os, std::string_view sv)
    {
        { os << sv } -> std::convertible_to<Os &>;
    };

    // Objects such as std::ofstream
    template <typename Os>
    concept StdLHROStreamable = LHROStreamable<Os> && std::convertible_to<Os*, std::ostream*>;

    // Objects such as std::string
    template <typename St>
    concept SVAppendable =
        requires(St &st, std::string_view sv)
    {
        st.append( sv.data(), sv.data() + sv.size() );
    };

    struct RenderState;

    // Heterogeneous lookup support
    struct string_hash {
        using is_transparent = void;
        size_t operator()(const char *txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        size_t operator()(std::string_view txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        size_t operator()(const std::string &txt) const {
            return std::hash<std::string>{}(txt);
        }
    };

    using partials_map = std::unordered_map<
        std::string, std::string, string_hash, std::equal_to<>>;

    using partials_view_map = std::unordered_map<
        std::string, std::string_view, string_hash, std::equal_to<>>;
}

/** Reference to output stream used by handlebars

    This class is used to internally pass an output stream to the
    handlebars environment.

    It allows many types to be used as output streams, including
    std::string, std::ostream, llvm::raw_string_ostream, and others.

 */
class MRDOX_DECL OutputRef
{
    friend class Handlebars;

    using fptr = void (*)(void * out, std::string_view sv);
    void * out_;
    fptr fptr_;
    std::size_t indent_ = 0;

    template<class St>
    static
    void
    append_to_output(void * out, std::string_view sv) {
        St& st = *static_cast<St*>( out );
        st.append( sv.data(), sv.data() + sv.size() );
    }

    // write to output
    template<class Os>
    static
    void
    write_to_output(void * out, std::string_view sv) {
        Os& os = *static_cast<Os*>( out );
        os.write( sv.data(), sv.size() );
    }

    // stream to output
    template<class Os>
    static
    void
    stream_to_output(void * out, std::string_view sv) {
        Os& os = *static_cast<Os*>( out );
        os << sv;
    }

    // stream to output
    static
    void
    noop_output(void *, std::string_view) {}

    // Noop constructor
    // Used as implementation detail by Handlebars environment
    OutputRef()
        : out_( nullptr )
        , fptr_( &noop_output )
    {}

    OutputRef&
    write_impl( std::string_view sv );

public:
    /** Constructor for std::string output

        @param st The string to append to
     */
    template<detail::SVAppendable St>
    requires std::same_as<typename St::value_type, char>
    OutputRef( St& st )
      : out_( &st )
      , fptr_( &append_to_output<St> )
    {
    }

    /** Constructor for llvm::raw_string_ostream output

        @param os The output stream to write to
     */
    template <detail::StdLHROStreamable Os>
    requires std::is_convertible_v<Os*, std::ostream*>
    OutputRef( Os& os )
        : out_( &os )
        , fptr_( &write_to_output<Os> )
    {
    }

    /** Constructor for std::ostream& output

        @param os The output stream to write to
     */
    template <detail::LHROStreamable Os>
    requires std::is_convertible_v<Os*, std::ostream*>
    OutputRef( Os& os )
        : out_( &os )
        , fptr_( &write_to_output<Os> )
    {
    }

    /** Write to output

        @param sv The string to write
        @return A reference to this object
     */
    OutputRef&
    operator<<( std::string_view sv )
    {
        return write_impl( sv );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    OutputRef&
    operator<<( char c )
    {
        return write_impl( std::string_view( &c, 1 ) );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    OutputRef&
    operator<<( char const * c )
    {
        return write_impl( std::string_view( c ) );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    template <class T>
    requires fmt::has_formatter<T, fmt::format_context>::value
    OutputRef&
    operator<<( T v )
    {
        std::string s = fmt::format( "{}", v );
        return write_impl( s );
    }

    void
    setIndent(std::size_t indent)
    {
        indent_ = indent;
    }

    std::size_t
    getIndent() const noexcept
    {
        return indent_;
    }
};

class HandlebarsCallback;

namespace helpers {
    void
    unless_fn(
        dom::Array const& args,
        HandlebarsCallback const& options);
}

/** Callback information for handlebars helpers

    This class is used to pass information about the current
    context to handlebars helpers.

    It allows the helpers to access the current context,
    the current output stream, and render the current block.

 */
class MRDOX_DECL HandlebarsCallback
{
private:
    using callback_type = std::function<
        void(
            OutputRef,
            dom::Value const& /* context */,
            dom::Object const& /* data */,
            dom::Object const& /* blockValues */,
            dom::Object const& /* blockValuePaths */)>;

    callback_type fn_;
    callback_type inverse_;
    dom::Value const* context_{ nullptr };
    OutputRef* output_{ nullptr };
    dom::Object const* data_;
    std::vector<dom::Value> ids_;
    dom::Object hash_;
    dom::Object hashIds_;
    std::string_view name_;
    std::vector<std::string_view> blockParamIds_;
    std::function<void(dom::Value, dom::Array const&)> const* logger_;
    detail::RenderState* renderState_;
    HandlebarsOptions const* opt_;
    friend class Handlebars;

    friend
    void
    helpers::unless_fn(
        dom::Array const& args,
        HandlebarsCallback const& options);

public:
    /** Render the block content with the specified context

        This function renders the block content with the specified context.

        For example, the following template calls the helper "list" with a block section:

        @code{.handlebars}
          {{#list people}}{{name}}{{/list}}
        @endcode

        In this context, this function would render the `{{name}}` block
        content with any specified context.

        To list all people, a helper such as "list" would usually call this function once for each
        person, passing the person's object as the context argument.

        If this is not a block helper, this function returns an empty string.

        @param context The context to render the block content with
        @return The rendered block content

     */
    std::string
    fn(dom::Value const& context) const;

    /** Render the block content with the specified context

        This overload renders the block directly to the OutputRef
        instead of generating a string.

        This overload is particularly helpful in C++ block helpers
        where allocating a string is unnecessary, including built-in
        helpers. Other helpers can still use the first overload that
        allocates the string to obtain the same results.

        @param out Reference to the output
        @param context The context to render the block content with

     */
    void
    fn(OutputRef out, dom::Value const& context) const;

    /** Render the block content with the original context
     */
    std::string
    fn() const {
        return fn( *context_ );
    }

    /** Render the block content with the original context

        @param out Reference to the output
     */
    void
    fn(OutputRef out) const {
        fn( out, *context_ );
    }

    /** Render the block content with specified private data and block parameters

        This is the most complete overload of the `fn` function. It allows
        the caller to specify the new private data and values for
        block parameters to use when rendering the block content.

        @param context The context to render the block content with
        @param data The private data to render the block content with
        @param blockParams The block parameters to render the block content with
        @return The rendered block content

     */
    std::string
    fn(dom::Value const& context,
       dom::Object const& data,
       dom::Array const& blockParams,
       dom::Array const& blockParamPaths) const;

    /** Render the block content with specified private data and block parameters

        This is the most complete overload of the `fn` function. It allows
        the caller to specify the new private data and values for
        block parameters to use when rendering the block content.

        @param out Reference to the output
        @param context The context to render the block content with
        @param data The private data to render the block content with
        @param blockParams The block parameters to render the block content with
        @return The rendered block content

     */
    void
    fn(OutputRef out,
       dom::Value const& context,
       dom::Object const& data,
       dom::Array const& blockParams,
       dom::Array const& blockParamPaths) const;

    /** Render the inverse block content with the specified context

        This function renders the inverse block content with the specified context.

        For example, the following template calls the helper "list" with a
        block section:

        @code{.handlebars}
         {{#list people}}{{name}}{{else}}No people{{/list}}
        @endcode

        In this context, this `inverse` function would render the
        `No people` block content with any specified context.

        When there are no people, a helper such as "list" would usually
        call this function once, passing the original context argument instead
        of a person on the list.

        @param context The context to render the block content with
        @return The rendered block content
     */
    std::string
    inverse(dom::Value const& context) const;

    /** Render the inverse block content with the specified context

        This overload renders the block directly to the OutputRef
        instead of generating a string.

        This overload is particularly helpful in C++ block helpers
        where allocating a string is unnecessary, including built-in
        helpers. Other helpers can still use the first overload that
        allocates the string to obtain the same results.

        @param out Reference to the output
        @param context The context to render the block content with

     */
    void
    inverse(OutputRef out, dom::Value const& context) const;

    /** Render the inverse block content with the original context
     */
    std::string
    inverse() const {
        return inverse( *context_ );
    }

    /** Render the inverse block content with the original context

        @param out Reference to the output
     */
    void
    inverse(OutputRef out) const {
        inverse( out, *context_ );
    }

    /** Render the inverse block content with private data and block parameters

        This is the most complete overload of the `inverse` function. It allows
        the caller to specify the new private data and values for
        block parameters to use when rendering the block content.

        @param context The context to render the block content with
        @param data The private data to render the block content with
        @param blockParams The block parameters to render the block content with
        @return The rendered block content

     */
    std::string
    inverse(
        dom::Value const& context,
        dom::Object const& data,
        dom::Array const& blockParams,
        dom::Array const& blockParamPaths) const;

    /** Render the inverse block content with private data and block parameters

        This is the most complete overload of the `inverse` function. It allows
        the caller to specify the new private data and values for
        block parameters to use when rendering the block content.

        @param out Reference to the output
        @param context The context to render the block content with
        @param data The private data to render the block content with
        @param blockParams The block parameters to render the block content with
        @return The rendered block content

     */
    void
    inverse(
        OutputRef out,
        dom::Value const& context,
        dom::Object const& data,
        dom::Array const& blockParamPaths,
        dom::Array const& blockParams) const;

    /** Determine if helper is being called from a block section

        This function returns `true` if the helper is being called from a
        block section.

        For example, the following template calls the helper "list" with a
        block section:

        @code{.handlebars}
           {{#list people}}{{name}}{{/list}}
        @endcode

        The helper "list" is called with a block section because it is
        called with a block of text.

        On the other hand, the following template calls the helper "year"
        without a block section:

        @code{.handlebars}
          {{year}}
        @endcode

        This function also enables helpers to adjust its behavior based on
        whether it is being called from a block section, where it would
        typically use the block content, or an expression, where it would
        typically use its arguments:

        @code{.handlebars}
          {{! using helper arguments }}
          {{loud "Hello"}}

          {{! using helper block content }}
          {{#loud}}hello{{/loud}}
        @endcode

        This function can be used from within the helper to determine which
        behavior is expected from the template.

        @return `true` if the helper is being called from a block section
     */
    bool
    isBlock() const
    {
        return static_cast<bool>(fn_);
    }

    /** Log a message

        Helpers can use this callback function to log messages.

        The behavior of this function can be overriden with handlebars hooks.

        The built-in helper "log" uses this function to log messages.

     */
    void
    log(dom::Value const& level, dom::Array const& args) const;

    /** Get the current context where the helper is being called

        This function returns the current handlebars context at the
        moment the helper is being called.

        In most cases, the context is an object. However, it can also
        other types, in which case they are accessed from the handlebars
        templates with the `this` keyword.

        @return The current handlebars context
     */
    dom::Value const&
    context() const {
        MRDOX_ASSERT(context_);
        return *context_;
    }

    /// Private data passed to the callback
    dom::Object const&
    data() const {
        return *data_;
    }

    /// Ids of the expression parameters
    std::vector<dom::Value>&
    ids() {
        return ids_;
    }

    /// Ids of the expression parameters
    std::vector<dom::Value> const&
    ids() const {
        return ids_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object&
    hash() {
        return hash_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object const&
    hash() const {
        return hash_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object&
    hashIds() {
        return hashIds_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object const&
    hashIds() const {
        return hashIds_;
    }

    /// Block parameters passed to the callback
    std::size_t
    blockParams() const {
        return blockParamIds_.size();
    }

    /** Name of the helper being called

        This is the name of the helper being called from the handlebars
        template.

        It is useful for debugging purposes, where the helper function name
        doesn't match the name the helper was registered with.

     */
    std::string_view
    name() const {
        return name_;
    }

    /** Get the output stream used by the environment to render the template

        This function returns an output stream where a helper can directly
        write its output.

        This is particularly useful for helpers that render long blocks,
        so that they can write directly to the output stream instead of
        building a string in dynamic memory before returning it.

        @return The output stream of the parent template environment context
     */
    OutputRef
    output() const {
        return *output_;
    }

    /** Lookup a property in an object

        Handlebars expressions can also use dot-separated paths to indicate
        nested object values.

        @code{.handlebars}
        {{person.firstname}} {{person.lastname}}
        @endcode

        This expression looks up the `person` property in the input object
        and in turn looks up the `firstname` and `lastname` property within
        the `person` object.

        Handlebars also supports a `/` syntax so you could write the above
        template as:

        @code{.handlebars}
        {{person/firstname}} {{person/lastname}}
        @endcode

        @param context The object to look up the property in
        @param path The path to the property to look up

        @return The value of the property, or nullptr if the property does not exist
        @return `true` if the property was defined, `false` otherwise
     */
    MRDOX_DECL
    std::pair<dom::Value, bool>
    lookupProperty(
        dom::Value const& context,
        dom::Value const& path) const;
};

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
    enum class HelperBehavior {
        NO_RENDER,
        RENDER_RESULT,
        RENDER_RESULT_NOESCAPE,
    };

    using helper_type = std::function<
        std::pair<dom::Value, HelperBehavior>(
            dom::Array const&, HandlebarsCallback const&)>;

    using helpers_map = std::unordered_map<
        std::string, helper_type, detail::string_hash, std::equal_to<>>;

    using partials_map = detail::partials_map;
    partials_map partials_;
    helpers_map helpers_;
    std::function<void(dom::Value, dom::Array const&)> logger_;

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
        HandlebarsOptions const& options = {}) const;

    std::string
    render(std::string_view templateText) const
    {
        dom::Object const& context = {};
        return render(templateText, context);
    }

    /** Render a handlebars template

        This function renders the specified handlebars template and
        writes the result to the specified output stream.

        The output stream can be any type convertible to OutputRef, which is
        a reference to a stream that can be written to with the << operator.

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
        HandlebarsOptions const& options = {}) const;

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
            partials_.erase(it);
    }

    /** Register a helper with arguments and callback parameters

        This function registers a helper with the handlebars environment.
        A helper is a function that can be called from a template.

        The helper function the parameters from the template as a
        dom::Array.

        This overload uses the canonical helper type, which is
        a function that returns a `dom::Value` and takes the following
        arguments:

        @li dom::Array const&: The list of arguments passed to the helper in
            the template.

        @li HandlebarsCallback const&: Contains information about the current
            context. This is useful for block helpers that want to change the
            current context or render the internal blocks.

        When the helper is used in an subexpression, the `dom::Value` return
        value is used as the intermediary result. When the helper is used in
        a block or a final expression, the `dom::Value` return value will be
        formatted to the output.

        @param name The name of the helper in the handlebars template
        @param helper The helper function

        @see https://handlebarsjs.com/guide/builtin-helpers.html
     */
    template <std::invocable<dom::Array const&, HandlebarsCallback const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        using R = std::invoke_result_t<F, dom::Array const&, HandlebarsCallback const&>;
        static_assert(
            std::same_as<R, void> ||
            std::convertible_to<R, dom::Value>);
        registerHelperImpl(name, helper_type([helper = std::forward<F>(helper)](
            dom::Array const& args, HandlebarsCallback const& options)
                -> std::pair<dom::Value, HelperBehavior> {
           if constexpr (std::convertible_to<R, dom::Value>)
           {
               return {helper(args, options), HelperBehavior::RENDER_RESULT};
           }
           else if constexpr (std::same_as<R, void>)
           {
               helper(args, options);
               return {nullptr, HelperBehavior::NO_RENDER};
           }
        }));
    }

    /** Register a helper with no callback parameters

        This convenience overload registers a helper with the signature

        @code{.cpp}
        `R(dom::Array const&)`
        @endcode

        where `R` is either `void` or any value convertible to `dom::Value`.

        When compared to the canonical helper signature, this overload
        registers helpers that ignore the callback parameter.

        This is useful for simple custom helpers that are meant to be used
        in subexpressions.

        @param name The name of the helper
        @param helper The helper function
     */
    template <std::invocable<dom::Array const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        using R = std::invoke_result_t<F, dom::Array const&>;
        static_assert(
            std::same_as<R, void> ||
            std::convertible_to<R, dom::Value>);
        registerHelperImpl(name, helper_type([helper = std::forward<F>(helper)](
            dom::Array const& args, HandlebarsCallback const&)
                -> std::pair<dom::Value, HelperBehavior> {
            if constexpr (std::convertible_to<R, dom::Value>)
            {
                return {helper(args), HelperBehavior::RENDER_RESULT};
            }
            else if constexpr (std::same_as<R, void>)
            {
                helper(args);
                return {nullptr, HelperBehavior::NO_RENDER};
            }
        }));
    }

    /** Register a nullary helper

        This convenience overload registers a helper with the signature

        @code{.cpp}
        `R()`
        @endcode

        where `R` is either `void` or `dom::Value`.

        When compared to the canonical helper signature, this overload
        registers helpers that require no arguments and can ignore the
        callback parameter.

        This is useful for custom helpers that return dynamic data:

        @code{.handlebars}
        {{ year }}
        @endcode

        @param name The name of the helper
        @param helper The helper function
     */
    template<std::invocable<> F>
    void
    registerHelper(std::string_view name, F &&helper) {
        using R = std::invoke_result_t<F>;
        static_assert(
            std::same_as<R, void> ||
            std::convertible_to<R, dom::Value>);
        registerHelperImpl(name, helper_type([helper = std::forward<F>(helper)](
                dom::Array const&, HandlebarsCallback const&)
                    -> std::pair<dom::Value, HelperBehavior> {
            if constexpr (std::convertible_to<R, dom::Value>)
            {
                return {helper(), HelperBehavior::RENDER_RESULT};
            }
            else if constexpr (std::same_as<R, void>)
            {
                helper();
                return {nullptr, HelperBehavior::NO_RENDER};
            }
        }));
    }

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
    registerLogger(std::function<void(dom::Value, dom::Array const&)> fn);

    struct Tag;

private:
    void
    registerHelperImpl(
        std::string_view name,
        helper_type const &helper);

    // render to ostream using extra partials from parent contexts
    void
    render_to(
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    void
    renderTag(
        Tag const& tag,
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    void
    renderBlock(
        std::string_view blockName,
        Handlebars::Tag const &tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state,
        bool isChainedBlock) const;

    void
    renderPartial(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    void
    renderDecorator(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    void
    renderExpression(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    void
    setupArgs(
        std::string_view expression,
        dom::Value const& context,
        detail::RenderState& state,
        dom::Array &args,
        HandlebarsCallback& cb,
        HandlebarsOptions const& opt) const;

    struct evalExprResult {
        dom::Value value;
        bool found = false;
        bool isLiteral = false;
        bool isSubexpr = false;
        bool fromBlockParams = false;
    };

    evalExprResult
    evalExpr(
        dom::Value const &context,
        std::string_view expression,
        detail::RenderState &state,
        HandlebarsOptions const& opt,
        bool evalLiterals) const;

    std::pair<helper_type const&, bool>
    getHelper(std::string_view name, bool isBlock) const;

    std::pair<std::string_view, bool>
    getPartial(
        std::string_view name,
        detail::RenderState const& state) const;
};

/** Determine if a value is truthy

    A JSON value is truthy if it is a boolean and is true, a number and not
    zero, or an non-empty string, array or object.

    @param arg The value to test

    @return True if the value is truthy, false otherwise

 */
MRDOX_DECL
bool
isTruthy(dom::Value const& arg);

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
MRDOX_DECL
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
MRDOX_DECL
dom::Object
createFrame(dom::Object const& parent);

/** Create a wrapper for a safe string.

    This string wrapper prevents the string from being escaped
    when the template is rendered.

    When a helper returns a safe string, it will be marked
    as safe and will not be escaped when rendered. The
    string will be rendered as if converted to a `dom::Value`
    and rendered as-is.

    When constructing the string that will be marked as safe, any
    external content should be properly escaped using the
    `escapeExpression` function to avoid potential security concerns.

    @param str The string to mark as safe
    @return The safe string wrapper

    @see https://handlebarsjs.com/api-reference/utilities.html#handlebars-safestring-string
 */
MRDOX_DECL
dom::Value
safeString(std::string_view str);

/** HTML escapes the specified string

    This function HTML escapes the specified string, making it safe for
    rendering as text within HTML content.

    Replaces `&`, `<`, `>`, `"`, `'`, ```, `=` with the HTML entity
    equivalent value for string values.

    The output of all expressions except for triple-braced expressions
    are passed through this method. Helpers should also use this method
    when returning HTML content via a SafeString instance, to prevent
    possible code injection.

    Helper values created by the SafeString function are left untouched
    by the template and are not passed through this function.

    @param str The string to escape
    @return The escaped string
 */
MRDOX_DECL
std::string
escapeExpression(
    std::string_view str);

/// @overload escapeExpression(std::string_view)
MRDOX_DECL
void
escapeExpression(
    OutputRef out,
    std::string_view str);

template <std::convertible_to<dom::Value> DomValue>
requires (!std::convertible_to<DomValue, std::string_view>)
std::string
escapeExpression(
    DomValue const& val)
{
    dom::Value v = val;
    if (v.isString())
    {
        return escapeExpression(std::string_view(v.getString()));
    }
    if (v.isObject() && v.getObject().exists("toHTML"))
    {
        dom::Value fn = v.getObject().find("toHTML");
        if (fn.isFunction()) {
            return toString(fn.getFunction()());
        }
    }
    if (v.isNull() || v.isUndefined())
    {
        return {};
    }
    return toString(v);
}


/** An error thrown or returned by Handlebars

    An error returned or thrown by Handlebars environment when
    an error occurs during template rendering.

    The error message will be the same as the error message
    returned by Handlebars.js.

    The object will also contain the line, column and position
    of the error in the template. These can be used to by the
    caller to provide more detailed error messages.
 */
struct HandlebarsError
    : public std::runtime_error
{
    static constexpr std::size_t npos = std::size_t(-1);
    std::size_t line = std::size_t(-1);
    std::size_t column = std::size_t(-1);
    std::size_t pos = std::size_t(-1);

    HandlebarsError(std::string_view msg)
        : std::runtime_error(std::string(msg)) {}

    HandlebarsError(
        std::string_view msg,
        std::size_t line_,
        std::size_t column_,
        std::size_t pos_)
        : std::runtime_error(fmt::format("{} - {}:{}", msg, line_, column_))
        , line(line_)
        , column(column_)
        , pos(pos_) {}
};

/** An expected value or error

    This class is used to return a value or error from a function.

    It allows the caller to check if the value is valid or if an
    error occurred without having to throw an exception.

    @tparam T The type of the value
 */
template <class T>
class HandlebarsExpected
{
    std::variant<T, HandlebarsError> value_;
public:
    /** Construct a valid value

        @param value The value
     */
    HandlebarsExpected(T const& value)
        : value_(value) {}

    /** Construct a valid value

        @param value The value
     */
    HandlebarsExpected(T&& value)
        : value_(std::move(value)) {}

    /** Construct an error

        @param error The error
     */
    HandlebarsExpected(HandlebarsError const& error)
        : value_(error) {}

    /** Construct an error

        @param error The error
     */
    HandlebarsExpected(HandlebarsError&& error)
        : value_(std::move(error)) {}

    /** Check if the value is valid

        @return True if the value is valid, false otherwise
     */
    bool
    has_value() const noexcept
    {
        return std::holds_alternative<T>(value_);
    }

    /** Check if the value is an error

        @return True if the value is an error, false otherwise
     */
    bool
    has_error() const noexcept
    {
        return std::holds_alternative<HandlebarsError>(value_);
    }

    /** Get the value

        @return The value

        @throws HandlebarsError if the value is an error
     */
    T const&
    value() const
    {
        if (has_error())
            throw std::get<HandlebarsError>(value_);
        return std::get<T>(value_);
    }

    /// @copydoc value()
    T&
    value()
    {
        if (error())
            throw std::get<HandlebarsError>(value_);
        return std::get<T>(value_);
    }

    /** Get the value

        @return The value

        @throws HandlebarsError if the value is an error
     */
    T const&
    operator*() const
    {
        return std::get<T>(value_);
    }

    /// @copydoc operator*() const
    T&
    operator*()
    {
        return std::get<T>(value_);
    }

    /** Get a pointer to the value

        @return The value

        @throws HandlebarsError if the value is an error
     */
    T const*
    operator->() const
    {
        return &value();
    }

    /// @copydoc operator->() const
    T*
    operator->()
    {
        return &value();
    }

    /** Get the error

        @return The error

        @throws std::logic_error if the value is not an error
     */
    HandlebarsError const&
    error() const
    {
        if (has_value())
            throw std::logic_error("value is not an error");
        return std::get<HandlebarsError>(value_);
    }

    /// @copydoc error()
    HandlebarsError&
    error()
    {
        if (has_value())
            throw std::logic_error("value is not an error");
        return std::get<HandlebarsError>(value_);
    }
};

namespace helpers {

/** Register all the built-in helpers into a Handlebars instance

    Individual built-in helpers can also be registered with the
    public `*_fn` functions in this namespace.

    This allows the user to override only some of the built-in
    helpers. In particular, this is important for mandatory
    helpers, such as `blockHelperMissing` and `helperMissing`.

    @see https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers
    @see https://handlebarsjs.com/guide/builtin-helpers.html

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerBuiltinHelpers(Handlebars& hbs);

/// "if" helper function
/**
 * You can use the if helper to conditionally render a block.
 * If its argument returns false, undefined, null, "", 0, or [],
 * Handlebars will not render the block.
 *
 * The if block helper has special logic where is uses the first
 * argument as a conditional but uses the block content itself as the
 * item to render.
 */
MRDOX_DECL
void
if_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// "unless" helper function
/**
 * You can use the unless helper as the inverse of the if helper. Its block
 * will be rendered if the expression returns a falsy value.
 */
MRDOX_DECL
void
unless_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);


/// "unless" helper function
/**
 * The with-helper allows you to change the evaluation context of template-part.
 */
MRDOX_DECL
void
with_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// "each" helper function
/**
 * You can iterate over a list or object using the built-in each helper.
 *
 * Inside the block, you can use {{this}} to reference the element being iterated over.
 */
MRDOX_DECL
void
each_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// "log" helper function
/**
 * The lookup helper allows for dynamic parameter resolution using Handlebars variables.
 */
MRDOX_DECL
dom::Value
lookup_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// "log" helper function
/**
 * The log helper allows for logging of context state while executing a template.
 */
MRDOX_DECL
void
log_fn(
    dom::Array const& conditional,
    HandlebarsCallback const& options);

/// "helperMissing" helper function
/**
 * The helperMissing helper defines a function to be called when:
 *
 * 1) a helper is not found by the name, and
 * 2) the helper does not match a context property, and
 * 3) there might be one or more arguments for the helper
 *
 * For instance,
 *
 * @code{.handlebars}
 * {{foo 1 2 3}}
 * @endcode
 *
 * where the context key foo is not a helper and not a defined key, will
 * call the helperMissing helper with the values 1, 2, 3 as its arguments.
 *
 * The default implementation of helperMissing is:
 *
 * 1) if there are no arguments, render nothing to indicate the undefined value
 * 2) if there are one or more arguments, in which case it seems like a
 *    function call was intended, throw an error indicating the missing helper.
 *
 * This default behavior can be overridden by registering a custom
 * helperMissing helper.
 */
void
helper_missing_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// "blockHelperMissing" helper function
/**
 * The blockHelperMissing helper defines a function to be called when:
 *
 * 1) a block helper is not found by the name, and
 * 2) the helper name matches a context property, and
 * 3) there are no arguments for the helper
 *
 * For instance,
 *
 * @code{.handlebars}
 * {{#foo}}
 *    Block content
 * {{/foo}}
 * @endcode
 *
 * where the context key foo is not a helper, will call the
 * blockHelperMissing helper with the value of foo as the first argument.
 *
 * The default implementation of blockHelperMissing is to render the block
 * with the usual logic used by mustache, where the context becomes the
 * value of the key or, if the value is an array, the context becomes
 * each element of the array as if the "each" helper has been called on it.
 *
 * This default behavior can be overridden by registering a custom
 * blockHelperMissing helper.
 */
void
block_helper_missing_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/// No operation helper
void
noop_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/** Register all the Antora helpers into a Handlebars instance

    This function registers all the helpers that are part of the
    default Antora UI.

    Individual Antora helpers can also be registered with the
    public `*_fn` functions in this namespace.

    Since the Antora helpers are not mandatory and include
    many functions not applicable to all applications,
    this allows the user to register only some of the Antora
    helpers.

    @see https://gitlab.com/antora/antora-ui-default/-/tree/master/src/helpers

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerAntoraHelpers(Handlebars& hbs);

/** Register string helpers into a Handlebars instance

    This function registers a number of common helpers that operate on
    strings. String helpers are particularly useful because most
    applications will need to manipulate strings for presentation
    purposes.

    All helpers can be used as either block helpers or inline helpers.
    When used as a block helper, the block content is used as the first
    argument to the helper function. When used as an inline helper, the
    first argument is the value of the helper.

    The helper names are inspired by the default string functions provided
    in multiple programming languages, such as Python and Javascript,
    for their default string types.

    The individual helpers are defined as an implementation detail and
    cannot be registered individually.

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerStringHelpers(Handlebars& hbs);

/** Register helpers to manipulate composite data types

    This function registers a number of common helpers that operate on
    Objects and Arrays. Object and Array helpers are particularly useful
    because most applications will need to manipulate Objects and Arrays
    to extract information from them, such as object keys or specific
    Array items known ahead of time.

    The helper names are inspired by the default functions provided
    in multiple programming languages for dictionaries, objects, and arrays,
    such as Python and Javascript, for their default types.

    The individual helpers are defined as an implementation detail and
    cannot be registered individually.

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerContainerHelpers(Handlebars& hbs);

/** "and" helper function
 *
 *  The "and" helper returns true if all of the values are truthy.
 */
MRDOX_DECL
bool
and_fn(dom::Array const& args);

/** "or" helper function
 *
 *  The "or" helper returns true if any of the values are truthy.
 */
MRDOX_DECL
bool
or_fn(dom::Array const& args);

/** "eq" helper function
 *
 *  The "eq" helper returns true if all of the values are equal.
 */
MRDOX_DECL
bool
eq_fn(dom::Array const& args);

/** "ne" helper function
 *
 *  The "ne" helper returns true if any of the values are not equal.
 */
MRDOX_DECL
bool
ne_fn(dom::Array const& args);

/** "not" helper function
 *
 *  The "not" helper returns true if not all of the values are truthy.
 */
MRDOX_DECL
bool
not_fn(dom::Array const& arg);

/** "increment" helper function
 *
 *  The "increment" helper adds 1 to the value if it's an integer and converts
 *  booleans to `true`. Other values are returned as-is.
 */
MRDOX_DECL
dom::Value
increment_fn(
    dom::Array const &args,
    HandlebarsCallback const& options);

/** "detag" helper function
 *
 *  The "detag" helper applies the regex expression "<[^>]+>" to the
 *  input to remove all HTML tags.
 */
MRDOX_DECL
std::string
detag_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/** "relativize" helper function
 *
 *  The "relativize" helper makes the first path relative to the second path.
 */
MRDOX_DECL
dom::Value
relativize_fn(
    dom::Array const& args,
    HandlebarsCallback const& options);

/** "year" helper function
 *
 *  The "year" helper returns the current year as an integer.
 */
MRDOX_DECL
int
year_fn();

} // helpers
} // mrdox
} // clang

#endif
