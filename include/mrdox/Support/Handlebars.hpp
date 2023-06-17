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
    bool noEscape = false;
};

struct HandlebarsCallback {
    /// The callback function type to render a list item in a block section
    std::function<
        std::string(
            llvm::json::Object const& /* subcontext / item */)> fn;

    /// The callback function type to render a list item in the else section
    std::function<
        std::string(
            llvm::json::Object const& /* subcontext / item */)> inverse;

    /// Extra key value pairs passed to the callback
    llvm::json::Object hashes;
};

/// A handlebars template engine
/**
    This class implements a handlebars template engine.
    The template text is rendered using the data provided
    as a JSON array. The result is returned as a string.

    @see https://handlebarsjs.com/

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

    std::unordered_map<std::string, std::string, string_hash, std::equal_to<>> partials_;
    std::unordered_map<std::string, helper_type, string_hash, std::equal_to<>> helpers_;

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

private:
    void
    renderTag(
        std::string_view tag,
        llvm::raw_string_ostream& out,
        std::string_view& templateText,
        llvm::json::Object const &data,
        HandlebarsOptions opt) const;

    llvm::json::Value
    evalExpr(
        llvm::json::Object const &data,
        std::string_view expression) const;
};

// Helper functions shared by Handlebars.cpp and Builder.cpp
bool
isTruthy(llvm::json::Value const& arg);

llvm::json::Value const*
findNested(
    llvm::json::Object const &data,
    std::string_view name);

namespace helpers {

/// Register all the standard helpers into a Handlebars instance
/**
   @see https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers

   @param hbs The Handlebars instance to register the helpers into
 */
void
registerStandardHelpers(Handlebars& hbs);

/// Register all the Antora helpers into a Handlebars instance
/**
   @see https://gitlab.com/antora/antora-ui-default/-/tree/master/src/helpers

   @param hbs The Handlebars instance to register the helpers into
 */
void
registerAntoraHelpers(Handlebars& hbs);

llvm::json::Value
and_fn(llvm::json::Array const& args);

llvm::json::Value
or_fn(llvm::json::Array const& args);

llvm::json::Value
eq_fn(llvm::json::Array const& args);

llvm::json::Value
ne_fn(llvm::json::Array const& args);

llvm::json::Value
not_fn(llvm::json::Value const& arg);

llvm::json::Value
increment_fn(llvm::json::Value const &arg);

llvm::json::Value
detag_fn(std::string_view html);

llvm::json::Value
relativize_fn(
    llvm::json::Object const& ctx0,
    llvm::json::Array const& args);

llvm::json::Value
year_fn();

// https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers
llvm::json::Value
if_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& conditional,
    HandlebarsCallback const& cb);

llvm::json::Value
unless_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& conditional,
    HandlebarsCallback const& cb);

llvm::json::Value
with_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

llvm::json::Value
each_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb);

}

} // mrdox
} // clang

#endif
