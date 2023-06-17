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
        "    'to_string', function(context)\n"
        "{\n"
        "   return JSON.stringify(context, null, 2);\n"
        "});");
    if(err)
        throw err;
}

Expected<std::string>
Builder::
operator()(NamespaceInfo const& I)
{
return std::string();
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "namespace.adoc.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        // build page
        js::Object page(scope);
        page.insert("kind", "namespace");
        page.insert("name", I.Name);
        {
            js::Array list(scope);
            for(auto const& id : I.Members)
                visit(corpus_.get<Info>(id),
                    [&](auto const& I)
                    {
                        insertMember(list, I);
                    });
            page.insert("members", list);
        }
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String adocText(*result);
    return std::string(adocText);
}

Expected<std::string>
Builder::
operator()(RecordInfo const& I)
{
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "record.adoc.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        js::Object page(scope);
        page.insert("kind", "record");
        page.insert("name", I.Name);
        {
            js::Array list(scope);
            for(auto const& id : I.Members)
                visit(corpus_.get<Info>(id),
                    [&](auto const& I)
                    {
                        insertMember(list, I);
                    });
            page.insert("members", list);
        }
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String adocText(*result);
    return std::string(adocText);
}

Expected<std::string>
Builder::
operator()(FunctionInfo const& I)
{
    Config const& config = corpus_.config;

    js::Scope scope(ctx_);
    auto Handlebars = scope.getGlobal("Handlebars");
    auto layoutDir = files::appendPath(config.addonsDir,
            "generator", "asciidoc", "layouts");
    auto pathName = files::appendPath(layoutDir, "function.adoc.hbs");
    auto fileText = files::getFileText(pathName);
    if(! fileText)
        return fileText.getError();
    js::Object options(scope);
    options.insert("noEscape", true);
    auto templateFn = Handlebars.call("compile", *fileText, options);

    js::Object context(scope);
    {
        js::Object page(scope);
        page.insert("kind", "record");
        page.insert("name", I.Name);
        page.insert("decl", renderFunctionDecl(I));
        if(I.DefLoc.has_value())
            page.insert("loc", I.DefLoc->Filename);
        if(I.javadoc)
            makeJavadoc(page, *I.javadoc);
        context.insert("page", page);
    }

    auto result = js::tryCall(templateFn, context);
    if(! result)
        return result.getError();
    js::String adocText(*result);
    return std::string(adocText);
}

//------------------------------------------------

/*  Append the short-form descriptor of I to the list.
*/
void
Builder::
insertMember(
    js::Array const& list, auto const& I)
{
    js::Scope scope(ctx_);
    js::Object obj(scope);
    obj.insert("name", I.Name);
    obj.insert("tag", I.symbolType());
    obj.insert("id", toBase16(I.id));
    if(I.javadoc)
        makeJavadoc(obj, *I.javadoc);
    list.push_back(obj);
}

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

std::string
Builder::
renderTypeName(
    TypeInfo const& I)
{
    if(I.id == SymbolID::zero)
        return I.Name;
    // VFALCO Work-around for having IDs that don't exist
    auto J = corpus_.find(I.id);
    if(J == nullptr)
        return I.Name;
    return J->Name;
}

std::string
Builder::
renderFormalParam(
    Param const& I)
{
    std::string s;
    s = renderTypeName(I.Type);
    if(! I.Name.empty())
    {
        s.push_back(' ');
        s.append(I.Name);
    }
    return s;
}

std::string
Builder::
renderFunctionDecl(
    FunctionInfo const& I)
{
    std::string dest;
    dest.append(renderTypeName(I.ReturnType));
    dest.push_back('\n');
    dest.append(I.Name);
    if(! I.Params.empty())
    {
        dest.append("(\n    ");
        dest.append(renderFormalParam(I.Params[0]));
        for(std::size_t i = 1; i < I.Params.size(); ++i)
        {
            dest.append(",\n    ");
            dest.append(renderFormalParam(I.Params[i]));
        }
        dest.append(");\n");
    }
    else
    {
        dest.append("();\n");
    }
    return dest;
}

//------------------------------------------------

dom::Object
Builder::
domGetSymbol(
    SymbolID const& id)
{
    return visit(corpus_.get(id),
        [&]<class T>(T const& I)
        {
            return dom::makeObject<
                Symbol<T>>(I, corpus_);
        });
}

//------------------------------------------------

} // adoc
} // mrdox
} // clang
