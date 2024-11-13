//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Builder.hpp"
#include "lib/Support/Radix.hpp"
#include <lib/Lib/ConfigImpl.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <fmt/format.h>
#include <filesystem>

namespace clang {
namespace mrdocs {

namespace lua {
extern void lua_dump(dom::Object const& obj);
}

namespace hbs {

Builder::
Builder(
    HandlebarsCorpus const& corpus)
    : domCorpus(corpus)
{
    namespace fs = std::filesystem;

    Config const& config = domCorpus->config;

    // load partials
    std::string partialsPath = files::appendPath(
        config->addons, "generator", domCorpus.fileExtension, "partials");
    forEachFile(partialsPath, true,
        [&](std::string_view pathName) -> Error
        {
            fs::path path = pathName;
            if(path.extension() != ".hbs")
                return Error::success();
            path = path.lexically_relative(partialsPath);
            while(path.has_extension())
                path.replace_extension();

            auto text = files::getFileText(pathName);
            if (! text)
                return text.error();

            hbs_.registerPartial(
                path.generic_string(), *text);
            return Error::success();
        }).maybeThrow();

    // Load JavaScript helpers
    std::string helpersPath = files::appendPath(
        config->addons, "generator", domCorpus.fileExtension, "helpers");
    forEachFile(helpersPath, true,
        [&](std::string_view pathName)-> Expected<void>
        {
            // Register JS helper function in the global object
            constexpr std::string_view ext = ".js";
            if (!pathName.ends_with(ext)) return {};
            auto name = files::getFileName(pathName);
            name.remove_suffix(ext.size());
            MRDOCS_TRY(auto script, files::getFileText(pathName));
            MRDOCS_TRY(js::registerHelper(hbs_, name, ctx_, script));
            return {};
        }).maybeThrow();

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

    helpers::registerStringHelpers(hbs_);
    helpers::registerAntoraHelpers(hbs_);
    helpers::registerLogicalHelpers(hbs_);
    helpers::registerContainerHelpers(hbs_);
}

//------------------------------------------------

Expected<std::string>
Builder::
callTemplate(
    std::string_view name,
    dom::Value const& context)
{
    Config const& config = domCorpus->config;

    auto layoutDir = files::appendPath(config->addons,
            "generator", domCorpus.fileExtension, "layouts");
    auto pathName = files::appendPath(layoutDir, name);
    MRDOCS_TRY(auto fileText, files::getFileText(pathName));
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

//------------------------------------------------

std::string
Builder::
getRelPrefix(std::size_t depth)
{
    Config const& config = domCorpus->config;

    std::string rel_prefix;
    if(! depth || ! config->legibleNames ||
        ! domCorpus->config->multipage)
        return rel_prefix;
    --depth;
    rel_prefix.reserve(depth * 3);
    while(depth--)
        rel_prefix.append("../");
    return rel_prefix;
}

dom::Object
Builder::
createContext(
    Info const& I)
{
    dom::Object ctx;
    ctx.set("symbol", domCorpus.get(I.id));
    ctx.set("relfileprefix", getRelPrefix(I.Namespace.size()));
    ctx.set("config", domCorpus->config.object());
    ctx.set("sectionref", domCorpus.names_.getQualified(I.id, '-'));
    return ctx;
}

dom::Object
Builder::
createContext(
    OverloadSet const& OS)
{
    dom::Object ctx;
    ctx.set("symbol", domCorpus.getOverloads(OS));
    const Info& Parent = domCorpus->get(OS.Parent);
    ctx.set("relfileprefix", getRelPrefix(Parent.Namespace.size() + 1));
    ctx.set("config", domCorpus->config.object());
    ctx.set("sectionref", domCorpus.names_.getQualified(OS, '-'));
    return ctx;
}

template<class T>
Expected<std::string>
Builder::
operator()(T const& I)
{
    auto const templateFile = fmt::format("index.{}.hbs", domCorpus.fileExtension);
    dom::Object ctx = createContext(I);

    auto& config = domCorpus->config;
    bool isSinglePage = !config->multipage;
    if (config->embedded ||
        isSinglePage)
    {
        return callTemplate(templateFile, ctx);
    }

    auto const wrapperFile = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object wrapperCtx = createFrame(ctx);
    wrapperCtx.set("contents", dom::makeInvocable([this, &I, templateFile](
        dom::Value const& options) -> Expected<dom::Value>
        {
            // Helper to write contents directly to stream
            return callTemplate(templateFile, createContext(I));
        }));
    return callTemplate(wrapperFile, wrapperCtx);
}

Expected<std::string>
Builder::
operator()(OverloadSet const& OS)
{
    auto const templateFile = fmt::format("index-overload-set.{}.hbs", domCorpus.fileExtension);
    dom::Object ctx = createContext(OS);

    auto& config = domCorpus->config;
    bool isSinglePage = !config->multipage;
    if (config->embedded ||
        isSinglePage)
    {
        return callTemplate(templateFile, ctx);
    }

    auto const wrapperFile = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object wrapperCtx = createFrame(ctx);
    wrapperCtx.set("contents", dom::makeInvocable([this, &OS, templateFile](
        dom::Value const& options) -> Expected<dom::Value>
        {
            // Helper to write contents directly to stream
            return callTemplate(templateFile, createContext(OS));
        }));
    return callTemplate(wrapperFile, wrapperCtx);
}

Expected<void>
Builder::
wrapPage(
    std::ostream& out,
    std::istream& in)
{
    auto const wrapperFile = fmt::format("wrapper.{}.hbs", domCorpus.fileExtension);
    dom::Object ctx;
    ctx.set("contents", dom::makeInvocable([&in](
         dom::Value const& options) -> Expected<dom::Value>
    {
        // Helper to write contents directly to stream
        // AFREITAS: custom functions should set options["write"]
        // to avoid creating a string.
        return std::string(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>());
    }));
    // Render directly to ostream
    Config const& config = domCorpus->config;
    auto layoutDir = files::appendPath(config->addons,
        "generator", domCorpus.fileExtension, "layouts");
    auto pathName = files::appendPath(layoutDir, wrapperFile);
    MRDOCS_TRY(auto fileText, files::getFileText(pathName));
    HandlebarsOptions options;
    options.noEscape = true;
    OutputRef outRef(out);
    Expected<void, HandlebarsError> exp =
        hbs_.try_render_to(outRef, fileText, ctx, options);
    if (!exp)
    {
        return Unexpected(Error(exp.error().what()));
    }
    return {};
}


// Define Builder::operator() for each Info type
#define DEFINE(T) template Expected<std::string> \
    Builder::operator()<T>(T const&)

#define INFO(Type) DEFINE(Type##Info);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

} // hbs
} // mrdocs
} // clang
