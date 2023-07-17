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

    js::Scope scope(ctx_);

    scope.script(files::getFileText(
        files::appendPath(
            config->addonsDir, "js", "handlebars.js")
        ).value()).maybeThrow();
    auto Handlebars = scope.getGlobal("Handlebars").value();

// VFALCO refactor this
Handlebars.setlog();

    // load templates
#if 0
    err = forEachFile(options_.template_dir,
        [&](std::string_view fileName)
        {
            if(! fileName.ends_with(".html.hbs"))
                return Error::success();
            return Error::success();
        });
#endif

    Error err;

    // load partials
    forEachFile(
        files::appendPath(config->addonsDir,
            "generator", "html", "partials"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".html.hbs";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            auto text = files::getFileText(pathName);
            if(! text)
                return text.error();
            return Handlebars.callProp(
                "registerPartial", name, *text).error();
        }).maybeThrow();

    // load helpers
#if 0
    err = forEachFile(
        files::appendPath(config->addonsDir,
            "generator", "js", "helpers"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".js";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            //Handlebars.callProp("registerHelper", name, *text);
            auto err = ctx_.scriptFromFile(pathName);
            return Error::success();
        }).maybeThrow();
#endif

    scope.script(fmt::format(
        R"(Handlebars.registerHelper(
            'is_multipage', function()
        {{
            return {};
        }});
        )", config->multiPage)).maybeThrow();

    scope.script(R"(
        Handlebars.registerHelper(
            'to_string', function(context, depth)
        {
            return JSON.stringify(context, null, 2);
        });

        Handlebars.registerHelper(
            'eq', function(a, b)
        {
            return a === b;
        });

        Handlebars.registerHelper(
            'neq', function(a, b)
        {
            return a !== b;
        });

        Handlebars.registerHelper(
            'not', function(a)
        {
            return ! a;
        });

        Handlebars.registerHelper(
            'or', function(a, b)
        {
            return a || b;
        });

        Handlebars.registerHelper(
            'and', function(a, b)
        {
            return a && b;
        });
    )").maybeThrow();
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
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.error();
    dom::Object options;
    options.set("allowProtoPropertiesByDefault", true);
    // VFALCO This makes Proxy objects stop working
    //options.set("allowProtoMethodsByDefault", true);
    auto templateFn = Handlebars->callProp("compile", *fileText, options);
    if(! templateFn)
        return templateFn.error();
    auto result = templateFn->call(context, options);
    if(! result)
        return result.error();
    return result->getString();
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
