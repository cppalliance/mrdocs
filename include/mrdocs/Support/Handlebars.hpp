//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_SUPPORT_PATH_HPP
#define MRDOCS_TOOL_SUPPORT_PATH_HPP

#include <mrdocs/Support/String.hpp>
#include <mrdocs/Dom.hpp>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <vector>
#include <variant>

namespace clang {
namespace mrdocs {

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
    std::size_t line = static_cast<std::size_t>(-1);
    std::size_t column = static_cast<std::size_t>(-1);
    std::size_t pos = static_cast<std::size_t>(-1);

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
        using is_transparent [[maybe_unused]] = void;
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
class MRDOCS_DECL OutputRef
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
    friend
    OutputRef&
    operator<<( OutputRef& os, std::string_view sv )
    {
        return os.write_impl( sv );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    friend
    OutputRef&
    operator<<( OutputRef& os, char c )
    {
        return os.write_impl( std::string_view( &c, 1 ) );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    friend
    OutputRef&
    operator<<( OutputRef& os, char const * c )
    {
        return os.write_impl( std::string_view( c ) );
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    template <class T>
    requires fmt::has_formatter<T, fmt::format_context>::value
    friend
    OutputRef&
    operator<<( OutputRef& os, T v )
    {
        std::string s = fmt::format( "{}", v );
        return os.write_impl( s );
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
        dom::Object const& context = {};
        auto exp = try_render(templateText, context, {});
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
    Expected<std::string, HandlebarsError>
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
    Expected<std::string, HandlebarsError>
    try_render(
        std::string_view templateText,
        dom::Value const& context) const
    {
        return try_render(templateText, context, {});
    }

    /// @overload
    Expected<std::string, HandlebarsError>
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

        @param templateText The handlebars template text
        @param context The data to render
        @param options The options to use
        @return The rendered text
     */
    Expected<void, HandlebarsError>
    try_render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context,
        HandlebarsOptions const& options) const;

    /// @overload
    Expected<void, HandlebarsError>
    try_render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const& context) const
    {
        return try_render_to(out, templateText, context, {});
    }

    /// @overload
    Expected<void, HandlebarsError>
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

    struct Tag;

private:
    // render to ostream using extra partials from parent contexts
    Expected<void, HandlebarsError>
    try_render_to_impl(
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, HandlebarsError>
    renderTag(
        Tag const& tag,
        OutputRef& out,
        dom::Value const &context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, HandlebarsError>
    renderBlock(
        std::string_view blockName,
        Handlebars::Tag const &tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state,
        bool isChainedBlock) const;

    Expected<void, HandlebarsError>
    renderPartial(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, HandlebarsError>
    renderDecorator(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, HandlebarsError>
    renderExpression(
        Handlebars::Tag const& tag,
        OutputRef &out,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        detail::RenderState& state) const;

    Expected<void, HandlebarsError>
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
    Expected<evalExprResult, HandlebarsError>
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
MRDOCS_DECL
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
MRDOCS_DECL
dom::Object
createFrame(dom::Object const& parent);

/// @overload
MRDOCS_DECL
dom::Object
createFrame(dom::Value const& parent);

/// @overload
dom::Object
createFrame(dom::Object const& child, dom::Object const& parent);

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
MRDOCS_DECL
std::string
escapeExpression(
    std::string_view str);

/// @overload escapeExpression(std::string_view)
MRDOCS_DECL
void
escapeExpression(
    OutputRef out,
    std::string_view str);

/// @overload escapeExpression(std::string_view)
MRDOCS_DECL
void
escapeExpression(
    OutputRef out,
    std::string_view str,
    HandlebarsOptions const& opt);

template <std::convertible_to<dom::Value> DomValue>
requires (!std::convertible_to<DomValue, std::string_view>)
std::string
escapeExpression(
    DomValue const& val)
{
    dom::Value v = val;
    if (v.isString())
    {
        return escapeExpression(v.getString().get());
    }
    if (v.isObject() && v.getObject().exists("toHTML"))
    {
        dom::Value fn = v.getObject().get("toHTML");
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
MRDOCS_DECL
void
registerBuiltinHelpers(Handlebars& hbs);

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
MRDOCS_DECL
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
MRDOCS_DECL
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
MRDOCS_DECL
void
registerContainerHelpers(Handlebars& hbs);

/** "and" helper function
 *
 *  The "and" helper returns true if all of the values are truthy.
 */
MRDOCS_DECL
bool
and_fn(dom::Array const& args);

/** "or" helper function
 *
 *  The "or" helper returns true if any of the values are truthy.
 */
MRDOCS_DECL
bool
or_fn(dom::Array const& args);

/** "eq" helper function
 *
 *  The "eq" helper returns true if all of the values are equal.
 */
MRDOCS_DECL
bool
eq_fn(dom::Array const& args);

/** "ne" helper function
 *
 *  The "ne" helper returns true if any of the values are not equal.
 */
MRDOCS_DECL
bool
ne_fn(dom::Array const& args);

/** "not" helper function
 *
 *  The "not" helper returns true if not all of the values are truthy.
 */
MRDOCS_DECL
bool
not_fn(dom::Array const& arg);

/** "increment" helper function
 *
 *  The "increment" helper adds 1 to the value if it's an integer and converts
 *  booleans to `true`. Other values are returned as-is.
 */
MRDOCS_DECL
dom::Value
increment_fn(dom::Value const& value);

/** "detag" helper function
 *
 *  The "detag" helper applies the regex expression "<[^>]+>" to the
 *  input to remove all HTML tags.
 */
MRDOCS_DECL
dom::Value
detag_fn(dom::Value html);

/** "relativize" helper function
 *
 *  The "relativize" helper makes the first path relative to the second path.
 */
MRDOCS_DECL
dom::Value
relativize_fn(dom::Value to, dom::Value from, dom::Value context);

/** "year" helper function
 *
 *  The "year" helper returns the current year as an integer.
 */
MRDOCS_DECL
int
year_fn();

} // helpers
} // mrdocs
} // clang

#endif
