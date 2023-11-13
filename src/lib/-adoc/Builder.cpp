//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Builder.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdocs {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace adoc {

Builder::
Builder(
    AdocCorpus const& corpus)
    : domCorpus(corpus)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Config const& config = domCorpus.getCorpus().config;

    // load partials
    std::string partialsPath = files::appendPath(
        config->addonsDir, "generator", "asciidoc", "partials");
    forEachFile(partialsPath,
        [&](std::string_view pathName) -> Error
        {
            constexpr std::string_view ext = ".adoc.hbs";
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
    hbs_.registerHelper("primary_location",
        dom::makeInvocable([](dom::Value const& v) ->
            dom::Value
        {
            dom::Value src_loc = v.get("loc");
            if(! src_loc)
                return nullptr;
            dom::Value decls = src_loc.get("decl");
            if(dom::Value def = src_loc.get("def"))
            {
                // for classes/enums, prefer the definition
                dom::Value kind = v.get("kind");
                if(kind == "record" || kind == "enum")
                    return def;

                // we only every want to use the definition
                // for non-tag types when no other declaration
                // exists
                if(! decls)
                    return def;
            }
            if(! decls.isArray())
                return nullptr;
            dom::Value first;
            // otherwise, use whatever declaration had docs.
            // if no declaration had docs, fallback to the
            // first declaration
            for(const dom::Value& loc : decls.getArray())
            {
                if(loc.get("documented"))
                    return loc;
                else if(! first)
                    first = loc;
            }
            return first;
        }));
    helpers::registerAntoraHelpers(hbs_);
    // helpers::registerStringHelpers(hbs_);
    helpers::registerContainerHelpers(hbs_);
}

//------------------------------------------------

Expected<std::string>
Builder::
callTemplate(
    std::string_view name,
    dom::Value const& context)
{
    Config const& config = domCorpus.getCorpus().config;

    auto layoutDir = files::appendPath(config->addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    MRDOCS_TRY(auto fileText, files::getFileText(pathName));
    HandlebarsOptions options;
    options.noEscape = true;
    // options.compat = true;

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
    return callTemplate("single-header.adoc.hbs", {});
}

Expected<std::string>
Builder::
renderSinglePageFooter()
{
    return callTemplate("single-footer.adoc.hbs", {});
}

//------------------------------------------------

dom::Value
Builder::
createContext(
    Info const& I)
{
    dom::Object::storage_type props;
    props.emplace_back("symbol", domCorpus.get(I.id));
    std::string rel_prefix;
    if(domCorpus.options.safe_names &&
        domCorpus.getCorpus().config->multiPage &&
        ! I.Namespace.empty())
    {
        auto depth = I.Namespace.size() - 1;
        rel_prefix.reserve(depth * 3);
        while(depth--)
            rel_prefix.append("../");
    }
    props.emplace_back("relfileprefix", std::move(rel_prefix));
    return dom::Object(std::move(props));
}

template<class T>
Expected<std::string>
Builder::
operator()(T const& I)
{
    return callTemplate(
        "single-symbol.adoc.hbs",
        createContext(I));
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
DEFINE(FriendInfo);
DEFINE(EnumeratorInfo);

} // adoc
} // mrdocs
} // clang
