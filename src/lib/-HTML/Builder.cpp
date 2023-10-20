//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Builder.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdox {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace html {

Builder::
Builder(
    DomCorpus const& domCorpus,
    Options const& options)
    : domCorpus_(domCorpus)
    , corpus_(domCorpus_.getCorpus())
    , options_(options)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Config const& config = corpus_.config;

    // load partials
    std::string partialsPath = files::appendPath(
        config->addonsDir, "generator", "html", "partials");
    forEachFile(partialsPath,
                [&](std::string_view pathName) -> Error
    {
        constexpr std::string_view ext = ".html.hbs";
        if (!pathName.ends_with(ext))
        {
            return Error::success();
        }
        auto name = files::getFileName(pathName);
        name.remove_suffix(ext.size());
        auto text = files::getFileText(pathName);
        if (!text)
        {
            return text.error();
        }
        hbs_.registerPartial(name, *text);
        return Error::success();
    }).maybeThrow();

    // load helpers
    js::Scope scope(ctx_);
    std::string helpersPath = files::appendPath(
        config->addonsDir, "generator", "asciidoc", "helpers");
    forEachFile(helpersPath,
                [&](std::string_view pathName)
    {
        // Register JS helper function in the global object
        constexpr std::string_view ext = ".js";
        if (!pathName.ends_with(ext))
        {
            return Error::success();
        }
        auto name = files::getFileName(pathName);
        name.remove_suffix(ext.size());
        auto text = files::getFileText(pathName);
        if (!text)
        {
            return text.error();
        }
        auto JSFn = scope.compile_function(*text);
        if (!JSFn)
        {
            return JSFn.error();
        }
        scope.getGlobalObject().set(name, *JSFn);

        // Register C++ helper that retrieves the helper
        // from the global object, converts the arguments,
        // and invokes the JS function.
        hbs_.registerHelper(name, dom::makeVariadicInvocable(
                                      [this, name=std::string(name)](
                                          dom::Array const& args) -> Expected<dom::Value>
        {
            // Get function from global scope
            js::Scope scope(ctx_);
            js::Value fn = scope.getGlobalObject().get(name);
            if (fn.isUndefined())
            {
                return Unexpected(Error("helper not found"));
            }
            if (!fn.isFunction())
            {
                return Unexpected(Error("helper is not a function"));
            }

            // Call function
            std::vector<dom::Value> arg_span;
            arg_span.reserve(args.size());
            for (auto const& arg : args)
            {
                arg_span.push_back(arg);
            }
            auto result = fn.apply(arg_span);
            if (!result)
            {
                return dom::Kind::Undefined;
            }

            // Convert result to dom::Value
            return result->getDom();
        }));
        return Error::success();
    }).maybeThrow();
    hbs_.registerHelper(
        "is_multipage",
        dom::makeInvocable([res = config->multiPage]() -> Expected<dom::Value> {
        return res;
    }));
    helpers::registerAntoraHelpers(hbs_);
}

//------------------------------------------------

Expected<std::string>
Builder::
callTemplate(
    std::string_view name,
    dom::Value const& context)
{
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);


    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config->addonsDir,
            "generator", "html", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    MRDOX_TRY(auto fileText, files::getFileText(pathName));
    HandlebarsOptions options;
    options.noEscape = true;

    Expected<std::string, HandlebarsError> exp =
        hbs_.try_render(fileText, context, options);
    if (!exp)
    {
        return Unexpected(Error(exp.error().what()));
    }
    return *exp;
}

Expected<std::string>
Builder::
renderSinglePageHeader()
{
    return callTemplate("single-header.html.hbs", {});
}

Expected<std::string>
Builder::
renderSinglePageFooter()
{
    return callTemplate("single-footer.html.hbs", {});
}

//------------------------------------------------

dom::Value
Builder::
createContext(
    SymbolID const& id)
{
    return dom::Object({
        { "symbol", domCorpus_.get(id) }
        });
}

template<class T>
Expected<std::string>
Builder::
operator()(T const& I)
{
    return callTemplate(
        "single-symbol.html.hbs",
        createContext(I.id));
}

#define DEFINE(T) template Expected<std::string> \
    Builder::operator()<T>(T const&)

DEFINE(NamespaceInfo);
DEFINE(RecordInfo);
DEFINE(FunctionInfo);
DEFINE(EnumInfo);
DEFINE(TypedefInfo);
DEFINE(VariableInfo);
DEFINE(FieldInfo);
DEFINE(SpecializationInfo);

} // html
} // mrdox
} // clang
