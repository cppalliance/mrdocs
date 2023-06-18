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
#include "DocVisitor.hpp"
#include "Dom.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>

namespace clang {
namespace mrdox {
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
            throw fileText.getError();
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
                return text.getError();
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

    err = scope.script(
        "Handlebars.registerHelper("
        "    'to_string', function(context, depth)\n"
        "{\n"
        "   return JSON.stringify(context, null, 2);\n"
        "});");
    if(err)
        throw err;
}

Expected<std::string>
Builder::
callTemplate(
    std::string_view name, dom::ObjectPtr const& context)
{
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    options.insert("allowProtoPropertiesByDefault", true);
    options.insert("allowProtoMethodsByDefault", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object hbCtx(scope);
    hbCtx.insert("page", js::Object(scope, context));

    auto result = js::tryCall(templateFn, hbCtx);
    if(! result)
        return result.getError();
    js::String adocText(*result);
    return std::string(adocText);
}

Expected<std::string>
Builder::
operator()(NamespaceInfo const& I)
{
    return callTemplate("namespace.adoc.hbs", domGetSymbol(I.id));
}

Expected<std::string>
Builder::
operator()(RecordInfo const& I)
{
    return callTemplate("record.adoc.hbs", domGetSymbol(I.id));
}

Expected<std::string>
Builder::
operator()(FunctionInfo const& I)
{
    return callTemplate("function.adoc.hbs", domGetSymbol(I.id));
}

//------------------------------------------------

void
Builder::
makeJavadoc(
    js::Object const& item,
    Javadoc const& jd)
{
    js::Scope scope(ctx_);
    js::Object obj(scope);

    std::string dest;

    // brief
    DocVisitor visitor(dest);
    dest.clear();
    if(auto brief = jd.getBrief())
    {
        visitor(*brief);
    }
    obj.insert("brief", dest);

    // description
    if(! jd.getBlocks().empty())
    {
        dest.clear();
        visitor(jd.getBlocks());
        if(! dest.empty())
            obj.insert("description", dest);
    }

    item.insert("doc", obj);
}

//------------------------------------------------

dom::Pointer<dom::Object>
Builder::
domGetSymbol(
    SymbolID const& id)
{
    return visit(corpus_.get(id),
        [&]<class T>(T const& I)
        {
            return dom::Pointer<dom::Object>(
                dom::makePointer<Symbol<T>>(I, corpus_));
        });
}

//------------------------------------------------

} // adoc
} // mrdox
} // clang
