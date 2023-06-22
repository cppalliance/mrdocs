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
#include "llvm/Support/JSON.h"
#include <string_view>
#include <unordered_map>
#include <functional>
#include <type_traits>

namespace clang {
namespace mrdox {

struct HandlebarsOptions {
    /// Escape HTML entities
    bool noHTMLEscape = false;
};

class MRDOX_DECL HandlebarsCallback {
private:
    std::function<std::string(llvm::json::Object const&)> fn_;
    std::function<std::string(llvm::json::Object const&)> inverse_;
    friend class Handlebars;
public:
    /// Render the block content with the specified context
    /**
     * This function renders the block content with the specified context.
     *
     * For example, the following template calls the helper "list" with a block section:
     *
     *   {{#list people}}{{name}}{{/list}}
     *
     * In this context, this function would render the {{name}} block content with
     * any specified context.
     *
     * To list all people, a helper such as "list" would usually call this function once for each
     * person, passing the person's object as the context argument.
     *
     * If this is not a block helper, this function returns an empty string.
     *
     * @param context The context to render the block content with
     * @return The rendered block content
     */
    [[nodiscard]]
    std::string
    fn(llvm::json::Object const& context) const {
        if (isBlock()) {
            return fn_(context);
        } else {
            return {};
        }
    }

    /// Create frame around value so that { "this": context } and call fn
    [[nodiscard]]
    std::string
    fn(llvm::json::Value const& context) const;

    /// Render the inverse block content with the specified context
    /**
     * This function renders the inverse block content with the specified context.
     *
     * For example, the following template calls the helper "list" with a block section:
     *
     *  {{#list people}}{{name}}{{else}}No people{{/list}}
     *
     * In this context, this function would render the {{else}}...{{/list}} block content with
     * any specified context.
     *
     * When there are no people, a helper such as "list" would usually call this function once, passing
     * the original context argument instead of a person on the list.
     *
     * @param context The context to render the block content with
     * @return The rendered block content
     */
    [[nodiscard]]
    std::string
    inverse(llvm::json::Object const& context) const{
        return inverse_(context);
    };

    /// Create frame around value so that { "this": context } and call inverse
    [[nodiscard]]
    std::string
    inverse(llvm::json::Value const& context) const;

    /// Determine if helper is being called from a block section
    /**
     * This function returns true if the helper is being called from a block section.
     *
     * For example, the following template calls the helper "list" with a block section:
     *
     *    {{#list people}}{{name}}{{/list}}
     *
     * The helper "list" is called with a block section because it is called with a block of text.
     * On the other hand, the following template calls the helper "year" without a block section:
     *
     *   {{year}}
     *
     * Helpers can be called with or without a block section. For example, the following template
     * calls the helper "loud" with a block section, and the same helper "loud" without a block
     * section:
     *
     *  {{#loud}}HELLO{{/loud}} {{loud "Hello"}}
     *
     * This function can be used from within the helper to determine which behavior is expected
     * from the template.
     *
     * @return true if the helper is being called from a block section
     */
    [[nodiscard]]
    bool isBlock() const {
        return static_cast<bool>(fn_);
    }

    /// Log a message
    /**
     * Helpers can use this callback function to log messages.
     *
     * The behavior of this function can be overriden with the handlebars hooks.
     */
    void log(
        llvm::json::Value const& level,
        llvm::json::Array const& args) const;

    // Return context from callback to simplify helper implementation
    // llvm::json::Object const&
    // context() const;

    // AFREITAS: we need extra overloads of fn and inverse for blockParams
    // (as |userId user|) and private data (@index, @key, ...) as frames.

    /// Extra key value pairs passed to the callback
    llvm::json::Object hashes;

    /// Block parameters passed to the callback
    llvm::json::Array blockParams;

    /// Name of the helper being called
    std::string_view name;
};

/// A handlebars template engine
/**
 *    This class implements a handlebars template engine.
 *    The template text is rendered using the data provided
 *    as a JSON array. The result is returned as a string.
 *
 *    @see https://handlebarsjs.com/
 */
class Handlebars {
    using helper_type = std::function<
        llvm::json::Value(
            llvm::json::Object const& /* context */,
            llvm::json::Array const& /* args */,
            HandlebarsCallback const& /* callback params */)>;

    struct string_hash {
        using is_transparent = void;
        [[nodiscard]] size_t operator()(const char *txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(std::string_view txt) const {
            return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(const std::string &txt) const {
            return std::hash<std::string>{}(txt);
        }
    };

    using partials_map = std::unordered_map<std::string, std::string, string_hash, std::equal_to<>>;
    using helpers_map = std::unordered_map<std::string, helper_type, string_hash, std::equal_to<>>;

    partials_map partials_;
    helpers_map helpers_;

public:
    /// Render a handlebars template
    /**
       This function renders the specified handlebars template

       @param templateText The handlebars template text
       @param data The data to render
       @param opt The options to use
       @return The rendered text
     */
    std::string
    render(
        std::string_view templateText,
        llvm::json::Object const &data,
        HandlebarsOptions opt = {}) const;

    /// Render a handlebars template
    /**
       This function renders the specified handlebars template

       @param templateText The handlebars template text
       @param data The data to render
       @param opt The options to use
       @return The rendered text
     */
    void
    render_to(
        llvm::raw_string_ostream& out,
        std::string_view templateText,
        llvm::json::Object const &data,
        HandlebarsOptions opt = {}) const;

    /// Register a partial
    /**
       This function registers a partial with the handlebars engine.

       A partial is a template that can be referenced from another
       template. The partial is rendered in the context of the
       template that references it.

       @see https://handlebarsjs.com/guide/partials.html

       @param name The name of the partial
       @param text The text of the partial
     */
    void
    registerPartial(std::string_view name, std::string_view text);

    /// Register a helper with context, arguments, and callback parameters
    /**
       This function registers a helper with the handlebars engine.

       A helper is a function that can be called from a template.

       The helper function is passed the current context and
       the parameters from the template. The context is a JSON
       object, and the parameters are a JSON array.

       It should return a JSON object, which is rendered in the
       context of the template that calls it.

       @see https://handlebarsjs.com/guide/builtin-helpers.html

       @param name The name of the helper
       @param helper The helper function
     */
    void
    registerHelper(std::string_view name, helper_type const &helper);

    // AFREITAS: registering helpers with all the parameters they support
    // is very tedious. The following functions are convenience overloads
    // that allow registering helpers with fewer parameters. However,
    // more work needs to be done to better generalize the
    // convenience registerHelper functions below.
    // These overloads can also generalize the error messages when the
    // number of parameters is incorrect, or the parameters don't have
    // the appropriate type, which is very helpful for correctness.

    /// Register a helper context and arguments
    /**
       This overload registers a helper with context and arguments only.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<llvm::json::Object const&, llvm::json::Array const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const& ctx,
            llvm::json::Array const& args,
            HandlebarsCallback const&) -> llvm::json::Value {
            return f(ctx, args);
        }));
    }

    /// Register a helper with arguments only
    /**
       This overload registers a helper with arguments only.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<llvm::json::Array const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const&) -> llvm::json::Value {
            return f(args);
        }));
    }

    /// Register a helper with a single JSON value argument
    /**
       This overload registers a helper that transforms a single JSON value.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<llvm::json::Value const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const&) -> llvm::json::Value {
            return f(args[0]);
        }));
    }

    /// Register a helper with single string argument
    /**
       This overload registers a helper that transforms a string.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<std::string_view> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const&) -> llvm::json::Value {
            return f(std::string_view(args[0].getAsString().value()));
        }));
    }

    /// Register a helper with no arguments
    /**
       This overload registers a helper that transforms a single JSON value.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const&,
            HandlebarsCallback const&) -> llvm::json::Value {
            return f();
        }));
    }

    /// Register a helper with arguments and callback
    /**
       This overload registers a helper with arguments only.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<llvm::json::Array const&, HandlebarsCallback const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const& cb) -> llvm::json::Value {
            return f(args, cb);
        }));
    }

    /// Register a helper with a single JSON value argument and callback
    /**
       This overload registers a helper that transforms a single JSON value.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<llvm::json::Value const&, HandlebarsCallback const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const& cb) -> llvm::json::Value {
            MRDOX_ASSERT(!args.empty());
            return f(args[0], cb);
        }));
    }

    /// Register a helper with single string argument and callback
    /**
       This overload registers a helper that transforms a string.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<std::string_view, HandlebarsCallback const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const& args,
            HandlebarsCallback const& cb) -> llvm::json::Value {
            return f(std::string_view(args[0].getAsString().value()), cb);
        }));
    }

    /// Register a helper with no arguments and callback
    /**
       This overload registers a helper that transforms a single JSON value.

       @tparam F The type of the helper function
       @param name The name of the helper
       @param helper The helper function
     */
    template <std::invocable<HandlebarsCallback const&> F>
    void
    registerHelper(std::string_view name, F&& helper)
    {
        registerHelper(name, helper_type([f = std::forward<F>(helper)](
            llvm::json::Object const&,
            llvm::json::Array const&,
            HandlebarsCallback const& cb) -> llvm::json::Value {
            return f(cb);
        }));
    }

    struct Tag;

private:
    // render to ostream using extra partials from parent contexts
    void
    render_to(
        llvm::raw_string_ostream& out,
        std::string_view templateText,
        llvm::json::Object const &data,
        HandlebarsOptions opt,
        partials_map& inlinePartials) const;

    void
    renderTag(
        Tag const& tag,
        llvm::raw_string_ostream& out,
        std::string_view& templateText,
        llvm::json::Object const &data,
        HandlebarsOptions opt,
        partials_map& inlinePartials) const;

    void
    renderBlock(
        std::string_view blockName,
        Handlebars::Tag const& tag,
        llvm::raw_string_ostream &out,
        std::string_view &templateText,
        llvm::json::Object const& data,
        HandlebarsOptions const& opt,
        Handlebars::partials_map &inlinePartials) const;

    void
    renderPartial(
        Handlebars::Tag const& tag,
        llvm::raw_string_ostream &out,
        std::string_view &templateText,
        llvm::json::Object const& data,
        HandlebarsOptions &opt,
        Handlebars::partials_map &inlinePartials) const;

    void
    renderDecorator(
        Handlebars::Tag const& tag,
        llvm::raw_string_ostream &out,
        std::string_view &templateText,
        llvm::json::Object const& data,
        Handlebars::partials_map &inlinePartials) const;

    void
    renderExpression(
        Handlebars::Tag const& tag,
        llvm::raw_string_ostream &out,
        std::string_view &templateText,
        llvm::json::Object const& data,
        HandlebarsOptions const& opt) const;

    void
    setupArgs(
        std::string_view expression,
        llvm::json::Object const& data,
        llvm::json::Array &args,
        HandlebarsCallback &cb) const;

    llvm::json::Value
    evalExpr(
        llvm::json::Object const &data,
        std::string_view expression) const;

    helper_type const&
    getHelper(std::string_view name, bool isBlock) const;
};

/// Determine if a JSON value is truthy
/**
 * A JSON value is truthy if it is a boolean and is true, a number and not
 * zero, or an non-empty string, array or object.
 *
 * @param arg The JSON value to test
 *
 * @return True if the value is truthy, false otherwise
 */
MRDOX_DECL
bool
isTruthy(llvm::json::Value const& arg);

/// Lookup a property in a JSON object
/**
   @param data The JSON object to look up the property in
   @param path The path to the property to look up

   @return The value of the property, or nullptr if the property does not exist
 */
MRDOX_DECL
llvm::json::Value const*
lookupProperty(
    llvm::json::Value const &data,
    std::string_view path);

namespace helpers {

/// Register all the standard helpers into a Handlebars instance
/**
   @see https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers

   @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerBuiltinHelpers(Handlebars& hbs);

/// Register all the Antora helpers into a Handlebars instance
/**
   @see https://gitlab.com/antora/antora-ui-default/-/tree/master/src/helpers

   @param hbs The Handlebars instance to register the helpers into
 */
MRDOX_DECL
void
registerAntoraHelpers(Handlebars& hbs);

MRDOX_DECL
llvm::json::Value
and_fn(llvm::json::Array const& args);

MRDOX_DECL
llvm::json::Value
or_fn(llvm::json::Array const& args);

MRDOX_DECL
llvm::json::Value
eq_fn(llvm::json::Array const& args);

MRDOX_DECL
llvm::json::Value
ne_fn(llvm::json::Array const& args);

MRDOX_DECL
llvm::json::Value
not_fn(llvm::json::Value const& arg);

MRDOX_DECL
llvm::json::Value
increment_fn(llvm::json::Value const &arg);

MRDOX_DECL
llvm::json::Value
detag_fn(std::string_view html);

MRDOX_DECL
llvm::json::Value
relativize_fn(
    llvm::json::Object const& ctx0,
    llvm::json::Array const& args);

MRDOX_DECL
llvm::json::Value
year_fn();

// https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers


/// "if" helper function
/**
 * You can use the if helper to conditionally render a block. If its argument returns false, undefined, null,
 * "", 0, or [], Handlebars will not render the block.
 */
MRDOX_DECL
llvm::json::Value
if_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

/// "unless" helper function
/**
 * You can use the unless helper as the inverse of the if helper. Its block will be rendered if the expression
 * returns a falsy value.
 */
MRDOX_DECL
llvm::json::Value
unless_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);


/// "unless" helper function
/**
 * The with-helper allows you to change the evaluation context of template-part.
 */
MRDOX_DECL
llvm::json::Value
with_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

/// "each" helper function
/**
 * You can iterate over a list or object using the built-in each helper.
 *
 * Inside the block, you can use {{this}} to reference the element being iterated over.
 */
MRDOX_DECL
llvm::json::Value
each_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

/// "log" helper function
/**
 * The lookup helper allows for dynamic parameter resolution using Handlebars variables.
 */
MRDOX_DECL
llvm::json::Value
lookup_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

/// "log" helper function
/**
 * The log helper allows for logging of context state while executing a template.
 */
MRDOX_DECL
llvm::json::Value
log_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& conditional,
    HandlebarsCallback const& cb);

/// No operation helper
llvm::json::Value
noop_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& /* args */,
    HandlebarsCallback const& cb);

} // helpers
} // mrdox
} // clang

#endif
