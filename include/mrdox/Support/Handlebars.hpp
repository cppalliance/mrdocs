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
#include <mrdox/Support/Dom.hpp>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <vector>

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
    bool noHTMLEscape = false;

    /** Templates will throw rather than ignore missing fields
     */
    bool strict = false;

    /** Disable the auto-indent feature

        By default, an indented partial-call causes the output of the
        whole partial being indented by the same amount.
     */
    bool preventIndent = false;

    /** Disables standalone tag removal when set to true

        When set, blocks and partials that are on their own line will not
        remove the whitespace on that line
     */
    bool ignoreStandalone = false;
};

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

/** Reference to output stream used by handlebars

    This class is used to internally pass an output stream to the
    handlebars engine.

    It allows many types to be used as output streams, including
    std::string, std::ostream, llvm::raw_string_ostream, and others.

 */
class MRDOX_DECL OutputRef
{
    friend class Handlebars;

    using fptr = void (*)(void * out, std::string_view sv);
    void * out_;
    fptr fptr_;

private:
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
    // Used as implementation detail by Handlebars engine
    OutputRef()
        : out_( nullptr )
        , fptr_( &noop_output )
    {}

public:
    /** Constructor for std::string output

        @param st The string to append to
     */
    template<SVAppendable St>
    requires std::same_as<typename St::value_type, char>
    OutputRef( St& st )
      : out_( &st )
      , fptr_( &append_to_output<St> )
    {
    }

    /** Constructor for llvm::raw_string_ostream output

        @param os The output stream to write to
     */
    template <StdLHROStreamable Os>
    requires std::is_convertible_v<Os*, std::ostream*>
    OutputRef( Os& os )
        : out_( &os )
        , fptr_( &write_to_output<Os> )
    {
    }

    /** Constructor for std::ostream& output

        @param os The output stream to write to
     */
    template <LHROStreamable Os>
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
        fptr_( out_, sv );
        return *this;
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    OutputRef&
    operator<<( char c )
    {
        fptr_( out_, std::string_view( &c, 1 ) );
        return *this;
    }

    /** Write to output

        @param c The character to write
        @return A reference to this object
     */
    OutputRef&
    operator<<( char const * c )
    {
        fptr_( out_, std::string_view( c ) );
        return *this;
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
        fptr_( out_, s );
        return *this;
    }
};

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
        void(OutputRef, dom::Value const&, dom::Object const&, dom::Object const&)>;

    callback_type fn_;
    callback_type inverse_;
    dom::Value const* context_{ nullptr };
    OutputRef* output_{ nullptr };
    dom::Object data_;
    std::vector<std::string_view> ids_;
    dom::Object hashes_;
    std::string_view name_;
    std::vector<std::string_view> blockParams_;
    friend class Handlebars;

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
       dom::Object const& blockValues) const;

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
       dom::Object const& blockValues) const;

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
        dom::Object const& blockValues) const;

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
        dom::Object const& blockValues) const;

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
    bool isBlock() const {
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
    dom::Object&
    data() {
        return data_;
    }

    /// Private data passed to the callback
    dom::Object const&
    data() const {
        return data_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object&
    hashes() {
        return hashes_;
    }

    /// Extra key value pairs passed to the callback
    dom::Object const&
    hashes() const {
        return hashes_;
    }

    /// Ids of the expression parameters
    std::vector<std::string_view>&
    ids() {
        return ids_;
    }

    /// Ids of the expression parameters
    std::vector<std::string_view> const&
    ids() const {
        return ids_;
    }

    /// Block parameters passed to the callback
    std::vector<std::string_view>&
    blockParams() {
        return blockParams_;
    }

    /// Block parameters passed to the callback
    std::vector<std::string_view> const&
    blockParams() const {
        return blockParams_;
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
};

/** A handlebars template engine environment

    This class implements a handlebars template engine environment.
    It is analogous to the complete state held by the handlebars.js
    module, including registered helpers and partials.

    A handlebars template can be rendered using the context data provided
    as a dom::Value, which is usually a dom::Object at the first level.

    The result can returned as a string or rendered directly to an
    stream.

    @see https://handlebarsjs.com/
 */
class Handlebars {
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

    using helper_type = std::function<
        dom::Value(dom::Array const&, HandlebarsCallback const&)>;

    using helpers_map = std::unordered_map<
        std::string, helper_type, string_hash, std::equal_to<>>;

    partials_map partials_;
    helpers_map helpers_;

public:
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
        dom::Value const & context,
        HandlebarsOptions options = {}) const;

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
        dom::Value const & context,
        HandlebarsOptions options = {}) const;

    /** Register a partial

        This function registers a partial with the handlebars engine.

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

        This function unregisters a partial with the handlebars engine.

        @param name The name of the partial
     */
    void
    unregisterPartial(std::string_view name) {
        auto it = partials_.find(name);
        if (it != partials_.end())
            partials_.erase(it);
    }

    /** Register a helper with arguments and callback parameters

        This function registers a helper with the handlebars engine.
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
    void
    registerHelper(
        std::string_view name,
        std::function<dom::Value(
            dom::Array const&, HandlebarsCallback const&)> const &helper);

    /** Register a helper with arguments and callback parameters

        This overload registers a helper that returns `void` or any value
        convertible to `dom::Value`.

        @param name The name of the helper
        @param helper The helper function
     */
    template <std::invocable<dom::Array const&, HandlebarsCallback const&> F>
    requires (!std::same_as<F, helper_type>)
    void
    registerHelper(std::string_view name, F&& helper)
    {
        using R = std::invoke_result_t<F, dom::Array const&, HandlebarsCallback const&>;
        static_assert(std::same_as<R, void> || std::convertible_to<R, dom::Value>);
        registerHelper(name, helper_type([helper = std::forward<F>(helper)](
            dom::Array const& args, HandlebarsCallback const& cb) -> dom::Value {
           if constexpr (!std::same_as<R, void>)
           {
               return helper(args, cb);
           }
           else
           {
               helper(args, cb);
               return nullptr;
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
        static_assert(std::same_as<R, void> || std::convertible_to<R, dom::Value>);
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            dom::Array const& args, HandlebarsCallback const&) -> dom::Value {
            if constexpr (!std::same_as<R, void>)
            {
                return f(args);
            }
            else
            {
                f(args);
                return nullptr;
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
        static_assert(std::same_as<R, void> || std::convertible_to<R, dom::Value>);
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
                dom::Array const&, HandlebarsCallback const &) -> dom::Value {
            if constexpr (!std::same_as<R, void>) {
                return f();
            } else
            {
                f();
                return nullptr;
            }
        }));
    }

    /** Unregister a helper

        This function unregisters a helper with the handlebars engine.

        @param name The name of the helper
     */
    void
    unregisterHelper(std::string_view name) {
        auto it = helpers_.find(name);
        if (it != helpers_.end())
            helpers_.erase(it);
    }

    struct Tag;

private:
    // render to ostream using extra partials from parent contexts
    void
    render_to(
        OutputRef& out,
        std::string_view templateText,
        dom::Value const &context,
        HandlebarsOptions opt,
        partials_map& inlinePartials,
        dom::Object const& private_data,
        dom::Object const& blockValues) const;

    void
    renderTag(
        Tag const& tag,
        OutputRef& out,
        std::string_view& templateText,
        dom::Value const &context,
        HandlebarsOptions opt,
        partials_map& inlinePartials,
        dom::Object const& private_data,
        dom::Object const& blockValues) const;

    void
    renderBlock(
        std::string_view blockName,
        Handlebars::Tag const &tag,
        OutputRef &out,
        std::string_view &templateText,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        Handlebars::partials_map &extra_partials,
        dom::Object const& data,
        dom::Object const& blockValues) const;

    void
    renderPartial(
        Handlebars::Tag const& tag,
        OutputRef &out,
        std::string_view &templateText,
        dom::Value const& data,
        HandlebarsOptions &opt,
        Handlebars::partials_map &inlinePartials,
        dom::Object const& private_data,
        dom::Object const& blockValues) const;

    void
    renderDecorator(
        Handlebars::Tag const& tag,
        OutputRef &out,
        std::string_view &templateText,
        dom::Value const& context,
        Handlebars::partials_map &inlinePartials,
        dom::Object const& private_data,
        dom::Object const& blockValues) const;

    void
    renderExpression(
        Handlebars::Tag const& tag,
        OutputRef &out,
        std::string_view &templateText,
        dom::Value const& context,
        HandlebarsOptions const& opt,
        dom::Object const& private_data,
        dom::Object const& blockValues) const;

    void
    setupArgs(
        std::string_view expression,
        dom::Value const& context,
        dom::Object const& data,
        dom::Object const& blockValues,
        dom::Array &args,
        HandlebarsCallback &cb) const;

    dom::Value
    evalExpr(
        dom::Value const &context,
        dom::Object const &data,
        dom::Object const &blockValues,
        std::string_view expression) const;

    helper_type const&
    getHelper(std::string_view name, bool isBlock) const;
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

    @param arg The value to test
    @return True if the value is empty, false otherwise

    @see https://mustache.github.io/mustache.5.html#Sections
 */
MRDOX_DECL
dom::Object
createFrame(dom::Object const& parent);

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

    @param data The object to look up the property in
    @param path The path to the property to look up

    @return The value of the property, or nullptr if the property does not exist
 */
MRDOX_DECL
dom::Value
lookupProperty(
    dom::Value const &data,
    std::string_view path);

/** Stringify a value as JSON

    This function converts a dom::Value to a string as if
    JSON.stringify() had been called on it.

    Recursive objects are identified.

    @param v The value to stringify
    @return The stringified value
 */
MRDOX_DECL
std::string
JSON_stringify(dom::Value const& value);

namespace helpers {

/** Register all the standard helpers into a Handlebars instance

    @see https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers
    @see https://handlebarsjs.com/guide/builtin-helpers.html

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerBuiltinHelpers(Handlebars& hbs);

/// "if" helper function
/**
 * You can use the if helper to conditionally render a block. If its argument returns false, undefined, null,
 * "", 0, or [], Handlebars will not render the block.
 */
MRDOX_DECL
dom::Value
if_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

/// "unless" helper function
/**
 * You can use the unless helper as the inverse of the if helper. Its block will be rendered if the expression
 * returns a falsy value.
 */
MRDOX_DECL
dom::Value
unless_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);


/// "unless" helper function
/**
 * The with-helper allows you to change the evaluation context of template-part.
 */
MRDOX_DECL
dom::Value
with_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

/// "each" helper function
/**
 * You can iterate over a list or object using the built-in each helper.
 *
 * Inside the block, you can use {{this}} to reference the element being iterated over.
 */
MRDOX_DECL
dom::Value
each_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

/// "log" helper function
/**
 * The lookup helper allows for dynamic parameter resolution using Handlebars variables.
 */
MRDOX_DECL
dom::Value
lookup_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

/// "log" helper function
/**
 * The log helper allows for logging of context state while executing a template.
 */
MRDOX_DECL
void
log_fn(
    dom::Array const& conditional,
    HandlebarsCallback const& cb);

/// No operation helper
dom::Value
noop_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);


/** Register all the Antora helpers into a Handlebars instance

    @see https://gitlab.com/antora/antora-ui-default/-/tree/master/src/helpers

    @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerAntoraHelpers(Handlebars& hbs);

MRDOX_DECL
dom::Value
and_fn(dom::Array const& args);

MRDOX_DECL
dom::Value
or_fn(dom::Array const& args);

MRDOX_DECL
dom::Value
eq_fn(dom::Array const& args);

MRDOX_DECL
dom::Value
ne_fn(dom::Array const& args);

MRDOX_DECL
dom::Value
not_fn(dom::Array const& arg);

MRDOX_DECL
dom::Value
increment_fn(
    dom::Array const &args,
    HandlebarsCallback const& cb);

MRDOX_DECL
dom::Value
detag_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

MRDOX_DECL
dom::Value
relativize_fn(
    dom::Array const& args,
    HandlebarsCallback const& cb);

MRDOX_DECL
dom::Value
year_fn();

} // helpers
} // mrdox
} // clang

#endif
