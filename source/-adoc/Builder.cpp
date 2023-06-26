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
#include "Support/Radix.hpp"
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdox {

namespace lua {
extern void lua_dump(dom::ObjectPtr const& obj);
}

namespace adoc {

Builder::
Builder(
    Corpus const& corpus,
    Options const& options)
    : corpus_(corpus)
    , options_(options)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    Config const& config = corpus.config;

    js::Scope scope(ctx_);

    {
        auto handlebarsPath = files::appendPath(
            config.addonsDir, "js", "handlebars.js");
        auto fileText = files::getFileText(handlebarsPath);
        if(! fileText)
            throw fileText.error();
        if(auto err = scope.script(*fileText))
            throw err;
    }
    auto Handlebars = scope.getGlobal("Handlebars");

    // load templates
#if 0
    err = forEachFile(options_.template_dir,
        [&](std::string_view fileName)
        {
            if(! fileName.ends_with(".adoc.hbs"))
                return Error::success();
            return Error::success();
        });
#endif

    Error err;

    // load partials
    err = forEachFile(
        files::appendPath(config.addonsDir,
            "generator", "asciidoc", "partials"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".adoc.hbs";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            auto text = files::getFileText(pathName);
            if(! text)
                return text.error();
            Handlebars.call("registerPartial", name, *text);
            return Error::success();
        });
    if(err)
        throw err;

    // load helpers
#if 0
    err = forEachFile(
        files::appendPath(config.addonsDir,
            "generator", "js", "helpers"),
        [&](std::string_view pathName)
        {
            constexpr std::string_view ext = ".js";
            if(! pathName.ends_with(ext))
                return Error::success();
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            //Handlebars.call("registerHelper", name, *text);
            auto err = ctx_.scriptFromFile(pathName);
            return Error::success();
        });
    if(err)
        throw err;
#endif

    err = scope.script(R"(
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
    )");
    if(err)
        throw err;
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
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.error();
    js::Object options(scope);
    options.insert("noEscape", true);
    options.insert("allowProtoPropertiesByDefault", true);
    options.insert("allowProtoMethodsByDefault", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

//auto v = context.getObject()->get("symbol");
//if(v.isObject()) lua::lua_dump(v.getObject());
    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.error();
    js::String adocText(*result);
    return std::string(adocText);
}

Expected<std::string>
Builder::
renderSinglePageHeader()
{
    auto obj = makeShared<dom::Object>();
    auto text = callTemplate("single-header.adoc.hbs", obj);
    return text;
}

Expected<std::string>
Builder::
renderSinglePageFooter()
{
    auto obj = makeShared<dom::Object>();
    auto text = callTemplate("single-footer.adoc.hbs", obj);
    return text;
}

//------------------------------------------------

dom::Value
Builder::
createContext(
    SymbolID const& id)
{
    return dom::createObject({
            { "document", "test" },
            { "symbol", domCreateInfo(id, corpus_) }
        });
}

template<class T>
Expected<std::string>
Builder::
operator()(T const& I)
{
    return callTemplate(
        "single-symbol.adoc.hbs",
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

} // adoc
} // mrdox
} // clang
